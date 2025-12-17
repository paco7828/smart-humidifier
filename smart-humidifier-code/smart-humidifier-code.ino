#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <DHT.h>
#include <ezButton.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "config.h"
#include "drawing-functions.h"

// Function prototypes
unsigned long calculateOptimalSleepTime(unsigned long now);
void prepareForDeepSleep(unsigned long sleepTimeMs);
void startBLEAdvertising();
void stopBLEAdvertising();
void wakeDisplay();
void sleepDisplay();
void turnOffDisplay();
void handleAutonomousMode(unsigned long now);
void handleTimedMode(unsigned long now);
void startHumidifier(unsigned long now);
void stopHumidifier();
void initDisplay();
void updateDisplay();
void sendBLE(String msg);
void processCommand(String cmd);

void setup() {
  rtcState.bootCount++;

  // GPIO hold enable
  gpio_hold_dis((gpio_num_t)HUMID_PIN);
  gpio_hold_dis((gpio_num_t)BUTTON_PIN);

  // Restore state from RTC memory
  if (rtcState.bootCount > 1) {
    currentMode = (Mode)rtcState.savedMode;
    humidityThreshold = rtcState.savedHumidityThreshold;
    timedInterval = rtcState.savedTimedInterval;
    timedDuration = rtcState.savedTimedDuration;
    lastTimedStart = rtcState.lastTimedStartTime;
    humidifyStartTime = rtcState.humidifyStartTime;
    isHumidifying = rtcState.wasHumidifying;
    displaySleepStartTime = rtcState.displaySleepStart;
    currentTemp = rtcState.lastTemp;
    currentHumidity = rtcState.lastHumidity;
  }

  // Initialize pins
  pinMode(HUMID_PIN, OUTPUT);
  pinMode(TFT_LED, OUTPUT);
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);

  // Initialize button
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  button.setDebounceTime(100);

  // Initialize DHT sensor
  dht.begin();

  // Initialize display
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);

  // Turn on display
  digitalWrite(TFT_LED, HIGH);
  tft.fillScreen(ST77XX_BLACK);

  // First boot => show data
  if (rtcState.bootCount == 1) {
    showSplashScreen();
  }

  initDisplay();
  displayState = DISPLAY_ON;
  lastDisplayWake = millis();

  // Read sensor values
  float newTemp = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  if (!isnan(newTemp)) currentTemp = newTemp;
  if (!isnan(newHumidity)) currentHumidity = newHumidity;

  lastSensorRead = millis();
  lastActivityTime = millis();
}

void loop() {
  unsigned long now = millis();

  // Update button state
  button.loop();

  // Handle button press
  if (button.isPressed()) {
    lastActivityTime = now;
    wakeDisplay();

    if (!isAdvertising) {
      startBLEAdvertising();
    }
  }

  // Process BLE commands
  if (receivedCommand.length()) {
    lastActivityTime = now;
    processCommand(receivedCommand);
    receivedCommand = "";
  }

  // Check BLE advertising timeout
  if (isAdvertising && advertisingStartTime > 0) {
    if (now >= advertisingStartTime) {
      unsigned long bleElapsed = now - advertisingStartTime;
      if (bleElapsed >= ADVERTISING_DURATION) {
        stopBLEAdvertising();
      }
    }
  }

  // Handle sensor reading
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    float newTemp = dht.readTemperature();
    float newHumidity = dht.readHumidity();

    if (!isnan(newTemp)) currentTemp = newTemp;
    if (!isnan(newHumidity)) currentHumidity = newHumidity;

    rtcState.lastTemp = currentTemp;
    rtcState.lastHumidity = currentHumidity;
    rtcState.lastSensorReadTime = lastSensorRead;
  }

  // Handle display state machine
  if (displayState == DISPLAY_ON) {
    if (now - lastDisplayWake >= DISPLAY_WAKE_DURATION) {
      sleepDisplay();
    }

    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
      lastDisplayUpdate = now;
      updateDisplay();
    }
  } else if (displayState == DISPLAY_SLEEPING) {
    if (now - displaySleepStartTime >= DISPLAY_WAKE_INTERVAL) {
      wakeDisplay();
    }
  }

  // Handle mode-specific logic
  if (currentMode == AUTONOMOUS) {
    handleAutonomousMode(now);
  } else {
    handleTimedMode(now);
  }

  // Power management
  if (displayState == DISPLAY_SLEEPING && !isAdvertising) {
    unsigned long sleepTime = calculateOptimalSleepTime(now);
    prepareForDeepSleep(sleepTime);
  }

  delay(10);
}

unsigned long calculateOptimalSleepTime(unsigned long now) {
  // Wait for humidification to end
  if (isHumidifying) {
    if (currentMode == AUTONOMOUS) {
      unsigned long runTime = now - humidifyStartTime;
      if (runTime < MIN_RUNTIME) {
        return MIN_RUNTIME - runTime;
      }
    } else if (currentMode == TIMED) {
      unsigned long elapsed = (now - lastTimedStart) / 1000;
      if (elapsed < timedDuration) {
        return (timedDuration - elapsed) * 1000;
      }
    }
    stopHumidifier();
  }
  return DISPLAY_WAKE_INTERVAL;
}

void prepareForDeepSleep(unsigned long sleepTimeMs) {
  // Save into RTC memory
  rtcState.wasSleeping = (displayState == DISPLAY_SLEEPING);
  rtcState.savedMode = currentMode;
  rtcState.savedHumidityThreshold = humidityThreshold;
  rtcState.savedTimedInterval = timedInterval;
  rtcState.savedTimedDuration = timedDuration;
  rtcState.lastTimedStartTime = lastTimedStart;
  rtcState.humidifyStartTime = humidifyStartTime;
  rtcState.wasHumidifying = isHumidifying;
  rtcState.displayState = displayState;
  rtcState.displaySleepStart = displaySleepStartTime;

  // Display off
  digitalWrite(TFT_LED, LOW);
  tft.writeCommand(ST77XX_SLPIN);
  delay(10);

  // Set desired state
  gpio_set_direction((gpio_num_t)HUMID_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)HUMID_PIN, isHumidifying ? 1 : 0);
  gpio_hold_en((gpio_num_t)HUMID_PIN);  // enable hold

  // Stop BLE advertisement
  if (isAdvertising || bleInitialized) {
    if (pAdvertising) {
      pAdvertising->stop();
    }
    BLEDevice::deinit(true);
    delay(100);
    isAdvertising = false;
    bleInitialized = false;
  }

  // GPIO wakeup config
  esp_deep_sleep_enable_gpio_wakeup(1 << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);

  // Set timer wakeup
  esp_sleep_enable_timer_wakeup(sleepTimeMs * 1000ULL);
  delay(100);
  esp_deep_sleep_start();
}

void startBLEAdvertising() {
  wakeDisplay();

  if (bleInitialized && pAdvertising) {
    pAdvertising->stop();
  }

  if (bleInitialized) {
    BLEDevice::deinit(true);
    delay(200);
    bleInitialized = false;
    pCallbacks = NULL;
  }

  BLEDevice::init(bleDeviceName);
  BLEDevice::setMTU(128);

  pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

  // Create callback object
  if (pCallbacks == NULL) {
    pCallbacks = new SmartHumidifierCallbacks();
  }
  pCharacteristic->setCallbacks(pCallbacks);
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  pAdvertising->start();

  bleInitialized = true;
  isAdvertising = true;
  advertisingStartTime = millis();
}

void stopBLEAdvertising() {
  if (isAdvertising && pAdvertising) {
    pAdvertising->stop();
    delay(100);
    isAdvertising = false;
  }

  if (bleInitialized) {
    BLEDevice::deinit(true);
    delay(200);
    bleInitialized = false;
  }
}

void wakeDisplay() {
  if (displayState == DISPLAY_OFF || displayState == DISPLAY_SLEEPING) {
    displayState = DISPLAY_ON;
    lastDisplayWake = millis();
    displaySleepStartTime = 0;
    lastDisplayUpdate = 0;
    tft.writeCommand(ST77XX_SLPOUT);
    delay(120);
    digitalWrite(TFT_LED, HIGH);
    initDisplay();
    displayInitialized = true;
    lastActivityTime = millis();
  }
}

void sleepDisplay() {
  if (displayState == DISPLAY_ON) {
    displayState = DISPLAY_SLEEPING;
    displaySleepStartTime = millis();

    if (isAdvertising) {
      stopBLEAdvertising();
    }

    tft.writeCommand(ST77XX_SLPIN);
    digitalWrite(TFT_LED, LOW);
    displayInitialized = false;
  }
}

void turnOffDisplay() {
  displayState = DISPLAY_OFF;
  digitalWrite(TFT_LED, LOW);
  tft.fillScreen(ST77XX_BLACK);
  displayInitialized = false;
}

void handleAutonomousMode(unsigned long now) {
  if (isnan(currentHumidity)) return;

  if (isHumidifying) {
    unsigned long runTime = now - humidifyStartTime;
    if (runTime < MIN_RUNTIME) return;

    if (currentHumidity > (humidityThreshold + hysteresis)) {
      stopHumidifier();
    }
  } else {
    if (currentHumidity < (humidityThreshold - hysteresis)) {
      startHumidifier(now);
    }
  }
}

void handleTimedMode(unsigned long now) {
  if (timedModeFirstCycle) {
    lastTimedStart = now;
    timedModeFirstCycle = false;
  }

  unsigned long elapsed = (now - lastTimedStart) / 1000;

  if (isHumidifying) {
    if (elapsed >= timedDuration) {
      stopHumidifier();
      lastTimedStart = now;
    }
  } else {
    if (elapsed >= timedInterval) {
      startHumidifier(now);
      lastTimedStart = now;
    }
  }
}

void startHumidifier(unsigned long now) {
  isHumidifying = true;
  humidifyStartTime = now;
  digitalWrite(HUMID_PIN, HIGH);
  lastActivityTime = now;
}

void stopHumidifier() {
  isHumidifying = false;
  digitalWrite(HUMID_PIN, LOW);
}

void initDisplay() {
  tft.fillScreen(ST77XX_BLACK);

  int yPos = 5;
  int iconX = 25;

  drawTemperatureIcon(iconX, yPos, ST77XX_YELLOW);
  yPos += 32;
  drawWaterDropIcon(iconX, yPos, ST77XX_BLUE);
  yPos += 40;
  drawTargetIcon(iconX, yPos, ST77XX_ORANGE);
  yPos += 32;
  drawModeIcon(iconX, yPos, ST77XX_MAGENTA);

  displayInitialized = true;

  prevTemp = -999;
  prevHumidity = -999;
  prevThreshold = -999;
  prevMode = (currentMode == AUTONOMOUS) ? TIMED : AUTONOMOUS;
  updateDisplay();
}

void updateDisplay() {
  if (!displayInitialized) {
    initDisplay();
    return;
  }

  int textX = 60;

  // Update temperature
  if (abs(currentTemp - prevTemp) > 0.05) {
    int yPos = 5;
    tft.fillRect(textX, yPos, 100, 16, ST77XX_BLACK);
    tft.setCursor(textX, yPos);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.print(currentTemp, 1);
    tft.write(247);
    tft.print("C");
    prevTemp = currentTemp;
  }

  // Update humidity
  if (abs(currentHumidity - prevHumidity) > 0.05) {
    int yPos = 37;
    tft.fillRect(textX, yPos, 100, 16, ST77XX_BLACK);
    tft.setCursor(textX, yPos);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.print(currentHumidity, 1);
    tft.print("%");
    prevHumidity = currentHumidity;
  }

  // Update threshold
  if (abs(humidityThreshold - prevThreshold) > 0.05) {
    int yPos = 69;
    tft.fillRect(textX, yPos, 100, 16, ST77XX_BLACK);
    tft.setCursor(textX, yPos);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.print(humidityThreshold, 0);
    tft.print("%");
    prevThreshold = humidityThreshold;
  }

  // Update mode
  if (currentMode != prevMode) {
    int yPos = 101;
    tft.fillRect(textX, yPos, 100, 16, ST77XX_BLACK);
    tft.setCursor(textX, yPos);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.print(currentMode == AUTONOMOUS ? "AUTO" : "TIMED");
    prevMode = currentMode;
  }

  // Draw BLE icon
  if (isAdvertising) {
    drawBluetoothIcon(130, 110, ST77XX_BLUE);
  } else {
    tft.fillRect(130, 110, 20, 16, ST77XX_BLACK);
  }
}

void sendBLE(String msg) {
  if (pCharacteristic && isAdvertising) {
    pCharacteristic->setValue(msg.c_str());
    pCharacteristic->notify();
    delay(10);
  }
}

void processCommand(String cmd) {
  String resp = "";

  if (cmd == "DON") {
    wakeDisplay();
    resp = "Display on";
  } else if (cmd == "DOFF") {
    turnOffDisplay();
    resp = "Display off";
  } else if (cmd == "AUTO") {
    currentMode = AUTONOMOUS;
    timedModeFirstCycle = true;
    resp = "Autonomous mode";
  } else if (cmd == "TIMED") {
    currentMode = TIMED;
    lastTimedStart = millis();
    timedModeFirstCycle = false;
    resp = "Timed mode\nInterval:" + String(timedInterval) + "s\nFor:" + String(timedDuration) + "s";
  } else if (cmd.startsWith("INTRVL")) {
    int val = cmd.substring(6).toInt();
    if (val < 300) {
      resp = "Minimum is 5 minutes (300 seconds)";
    } else if (val > 14400) {
      resp = "Maximum is 4 hours (14400 seconds)";
    } else if (val <= timedDuration) {
      resp = "Value can't be less than or equal to FOR (" + String(timedDuration) + ")";
    } else {
      timedInterval = val;
      resp = "Interval OK: " + String(val) + "s";
    }
  } else if (cmd.startsWith("FOR")) {
    int val = cmd.substring(3).toInt();
    if (val < 300) {
      resp = "Minimum is 5 minutes (300 seconds)";
    } else if (val > 1800) {
      resp = "Maximum is 30 minutes (1800 seconds)";
    } else if (val >= timedInterval) {
      resp = "Value can't be more than or equal to INTRVL (" + String(timedInterval) + ")";
    } else {
      timedDuration = val;
      resp = "Duration OK: " + String(val) + "s";
    }
  } else if (cmd == "BLEON") {
    if (!isAdvertising) {
      startBLEAdvertising();
      resp = "BLE Advertising started";
    } else {
      resp = "BLE already running";
    }
  } else if (cmd == "BLEOFF") {
    stopBLEAdvertising();
    resp = "BLE Advertising stopped";
  } else if (cmd.startsWith("THRESH")) {
    float val = cmd.substring(6).toFloat();
    if (val < 20.0 || val > 80.0) {
      resp = "Threshold must be between 20% and 80%";
    } else {
      humidityThreshold = val;
      resp = "Threshold set to " + String(val, 1) + "%";
    }
  } else {
    resp = "Unknown command: " + cmd;
  }

  sendBLE(resp);
  lastActivityTime = millis();
}