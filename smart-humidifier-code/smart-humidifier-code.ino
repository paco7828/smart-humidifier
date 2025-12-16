#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <DHT.h>

/*
RGB Led functionality:
  -When humidifying needed in timed mode => pulsing red | else => pulsing green
  -When bluetooth client is present => pulsing blue
  -When known bluetooth command received => double green "beep" | else => double red "beep"
*/

// Pins
constexpr uint8_t TFT_CS = 7;
constexpr uint8_t TFT_RST = 2;
constexpr uint8_t TFT_DC = 3;
constexpr uint8_t TFT_LED = 0;
constexpr uint8_t DHT_PIN = 1;
constexpr uint8_t HUMID_PIN = 5;

// Temperature & humidity
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// Display
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// BLE
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
BLECharacteristic *pCharacteristic = NULL;
String receivedCommand = "";

class SmartHumidifierCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue().c_str();
    if (value.length() > 0) {
      receivedCommand = value;
      receivedCommand.trim();
    }
  }
};

// Operating modes
enum Mode {
  AUTONOMOUS,
  TIMED
};
Mode currentMode = AUTONOMOUS;  // default mode

// Autonomous mode
float humidityThreshold = 50.0;
float hysteresis = 5.0;
bool isHumidifying = false;
unsigned long humidifyStartTime = 0;
constexpr unsigned long MIN_RUNTIME = 300000;  // 5 minutes

// Timed mode
unsigned long timedInterval = 3600;  // 1 hour (seconds)
unsigned long timedDuration = 300;   // 5 minutes (seconds)
unsigned long lastTimedStart = 0;

// Display
bool displayOn = true;
unsigned long lastDisplayUpdate = 0;
constexpr unsigned long DISPLAY_UPDATE_INTERVAL = 2000;  // 2 seconds

// Temperature & humidity
float currentTemp = 0;
float currentHumidity = 0;
unsigned long lastSensorRead = 0;
constexpr unsigned long SENSOR_READ_INTERVAL = 2000;  // 2 seconds

// Previous display values for change detection
float prevTemp = -999;
float prevHumidity = -999;
float prevThreshold = -999;
Mode prevMode = AUTONOMOUS;
bool displayInitialized = false;

// RGB
uint8_t rgbR = 0, rgbG = 0, rgbB = 0;

void setup() {
  Serial.begin(115200);

  // BLE Setup
  BLEDevice::init("Smart-Humidifier");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pCharacteristic->setCallbacks(new SmartHumidifierCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  dht.begin();

  // Read sensor once immediately for initial display values
  currentTemp = dht.readTemperature();
  currentHumidity = dht.readHumidity();
  lastSensorRead = millis();

  // Humidifier
  pinMode(HUMID_PIN, OUTPUT);
  digitalWrite(HUMID_PIN, LOW);

  // Display
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);

  showSplashScreen();
  initDisplay();

  // Initialize display timer
  lastDisplayUpdate = millis();
}

void loop() {
  unsigned long now = millis();

  // BLE Command
  if (receivedCommand.length() > 0) {
    processCommand(receivedCommand);
    receivedCommand = "";
  }

  // Temperature & humidity
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    currentTemp = dht.readTemperature();
    currentHumidity = dht.readHumidity();
  }

  // Mode handling
  if (currentMode == AUTONOMOUS) {
    handleAutonomousMode(now);
  } else {
    handleTimedMode(now);
  }

  // Display
  if (displayOn && (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)) {
    lastDisplayUpdate = now;
    updateDisplay();
  }
}

void handleAutonomousMode(unsigned long now) {
  // Wrong humidity value
  if (isnan(currentHumidity))
    return;

  unsigned long runTime = now - humidifyStartTime;

  // Humidification with hysteresis
  if (isHumidifying) {
    if (runTime < MIN_RUNTIME)
      return;
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
  unsigned long elapsed = (now - lastTimedStart) / 1000;

  if (isHumidifying) {
    if (elapsed >= timedDuration)
      stopHumidifier();
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
}

void stopHumidifier() {
  isHumidifying = false;
  digitalWrite(HUMID_PIN, LOW);
}

void initDisplay() {
  tft.fillScreen(ST77XX_BLACK);

  int yPos = 5;
  int iconX = 25;

  // Draw all icons
  drawTemperatureIcon(iconX, yPos, ST77XX_YELLOW);
  yPos += 32;
  drawWaterDropIcon(iconX, yPos, ST77XX_BLUE);
  yPos += 40;
  drawTargetIcon(iconX, yPos, ST77XX_ORANGE);
  yPos += 32;
  drawModeIcon(iconX, yPos, ST77XX_MAGENTA);

  displayInitialized = true;

  // Force value update
  prevTemp = -999;
  prevHumidity = -999;
  prevThreshold = -999;
  prevMode = (currentMode == AUTONOMOUS) ? TIMED : AUTONOMOUS;
  updateDisplay();
}

void updateDisplay() {
  if (!displayInitialized) {
    initDisplay();
  }

  int textX = 60;

  // Update temperature if changed
  if (abs(currentTemp - prevTemp) > 0.05) {
    int yPos = 5;
    // Clear previous value area
    tft.fillRect(textX, yPos, 100, 16, ST77XX_BLACK);
    // Draw new value
    tft.setCursor(textX, yPos);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.print(currentTemp, 1);
    tft.write(247);
    tft.print("C");
    prevTemp = currentTemp;
  }

  // Update humidity if changed
  if (abs(currentHumidity - prevHumidity) > 0.05) {
    int yPos = 37;
    // Clear previous value area
    tft.fillRect(textX, yPos, 100, 16, ST77XX_BLACK);
    // Draw new value
    tft.setCursor(textX, yPos);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.print(currentHumidity, 1);
    tft.print("%");
    prevHumidity = currentHumidity;
  }

  // Update threshold if changed
  if (abs(humidityThreshold - prevThreshold) > 0.05) {
    int yPos = 77 - 8;
    // Clear previous value area
    tft.fillRect(textX, yPos, 100, 16, ST77XX_BLACK);
    // Draw new value
    tft.setCursor(textX, yPos);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.print(humidityThreshold, 0);
    tft.print("%");
    prevThreshold = humidityThreshold;
  }

  // Update mode if changed
  if (currentMode != prevMode) {
    int yPos = 109 - 8;
    // Clear previous value area
    tft.fillRect(textX, yPos, 100, 16, ST77XX_BLACK);
    // Draw new value
    tft.setCursor(textX, yPos);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.print(currentMode == AUTONOMOUS ? "AUTO" : "TIMED");
    prevMode = currentMode;
  }
}

void sendBLE(String msg) {
  if (pCharacteristic) {
    pCharacteristic->setValue(msg.c_str());
    pCharacteristic->notify();
  }
  Serial.println(msg);
}

void processCommand(String cmd) {
  String resp = "";

  // RGB (RRR;GGG;BBB)
  if (cmd.indexOf(';') >= 0) {
    int s1 = cmd.indexOf(';');
    int s2 = cmd.indexOf(';', s1 + 1);
    if (s2 > 0) {
      int r = cmd.substring(0, s1).toInt();
      int g = cmd.substring(s1 + 1, s2).toInt();
      int b = cmd.substring(s2 + 1).toInt();

      if (r < 0 || g < 0 || b < 0) {
        resp = "Minimum value is 0!";
      } else if (r > 255 || g > 255 || b > 255) {
        resp = "Maximum value is 255!";
      } else {
        rgbR = r;
        rgbG = g;
        rgbB = b;
        resp = "RGB OK";
      }
    }
  } else if (cmd == "DON") {
    displayOn = true;
    digitalWrite(TFT_LED, HIGH);
    resp = "Display on";
    initDisplay();
  } else if (cmd == "DOFF") {
    displayOn = false;
    digitalWrite(TFT_LED, LOW);
    displayInitialized = false;
    resp = "Display off";
  } else if (cmd == "AUTO") {
    currentMode = AUTONOMOUS;
    resp = "Autonomous mode";
  } else if (cmd == "TIMED") {
    currentMode = TIMED;
    lastTimedStart = millis();
    resp = "Timed mode\nInterval:" + String(timedInterval) + "\nFor:" + String(timedDuration) + "\n";
  } else if (cmd.startsWith("INTRVL")) {
    int val = cmd.substring(6).toInt();
    if (val < 300)
      resp = "Minimum is 5 minutes (300 seconds)";
    else if (val > 14400)
      resp = "Maximum is 4 hours (14400 seconds)";
    else if (val <= timedDuration)
      resp = "Value can't be less than or equal to FOR (" + String(timedDuration) + ")";
    else {
      timedInterval = val;
      resp = "Interval OK";
    }
  } else if (cmd.startsWith("FOR")) {
    int val = cmd.substring(3).toInt();
    if (val < 300)
      resp = "Minimum is 5 minutes (300 seconds)";
    else if (val > 1800)
      resp = "Maximum is 30 minutes (1800 seconds)";
    else if (val >= timedInterval)
      resp = "Value can't be more than or equal to INTRVL (" + String(timedInterval) + ")";
    else {
      timedDuration = val;
      resp = "Duration OK";
    }
  } else {
    resp = "Unknown command";
  }

  sendBLE(resp);
  Serial.println(cmd + " -> " + resp);
}


void drawTemperatureIcon(int x, int y, uint16_t color) {
  tft.fillCircle(x, y + 18, 7, color);   // Bulb
  tft.fillRect(x - 3, y, 6, 18, color);  // Tube
  // Mercury
  tft.fillRect(x - 1, y + 4, 2, 14, ST77XX_RED);
  tft.fillCircle(x, y + 18, 4, ST77XX_RED);
}

void drawWaterDropIcon(int x, int y, uint16_t color) {
  tft.fillCircle(x, y + 15, 6, color);
  tft.fillTriangle(x, y, x - 6, y + 15, x + 6, y + 15, color);
}

void drawModeIcon(int x, int y, uint16_t color) {
  tft.drawCircle(x, y, 7, color);
  tft.fillCircle(x, y, 2, color);
  tft.drawLine(x - 10, y, x - 7, y, color);
  tft.drawLine(x + 7, y, x + 10, y, color);
  tft.drawLine(x, y - 10, x, y - 7, color);
  tft.drawLine(x, y + 7, x, y + 10, color);
  tft.drawLine(x - 7, y - 7, x - 10, y - 10, color);
  tft.drawLine(x + 7, y + 7, x + 10, y + 10, color);
  tft.drawLine(x + 7, y - 7, x + 10, y - 10, color);
  tft.drawLine(x - 7, y + 7, x - 10, y + 10, color);
}

void drawTargetIcon(int x, int y, uint16_t color) {
  tft.drawCircle(x, y, 7, color);
  tft.drawCircle(x, y, 4, color);
  tft.fillCircle(x, y, 2, color);
  // Crosshair
  tft.drawLine(x - 10, y, x - 8, y, color);
  tft.drawLine(x + 8, y, x + 10, y, color);
  tft.drawLine(x, y - 10, x, y - 8, color);
  tft.drawLine(x, y + 8, x, y + 10, color);
}

void showSplashScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);

  int16_t x1, y1;
  uint16_t w1, h1;
  tft.getTextBounds("Smart", 0, 0, &x1, &y1, &w1, &h1);
  int xPos1 = (160 - w1) / 2;
  int yPos1 = (128 / 2) - h1 - 5;

  tft.setCursor(xPos1, yPos1);
  tft.setTextColor(ST77XX_CYAN);
  tft.println("Smart");

  tft.getTextBounds("Humidifier", 0, 0, &x1, &y1, &w1, &h1);
  int xPos2 = (160 - w1) / 2;
  int yPos2 = yPos1 + h1 + 5;

  tft.setCursor(xPos2, yPos2);
  tft.setTextColor(ST77XX_GREEN);
  tft.println("Humidifier");

  delay(1500);
}