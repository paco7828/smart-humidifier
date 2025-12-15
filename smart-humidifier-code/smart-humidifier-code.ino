#include <BluetoothSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <DHT.h>

// Pins
constexpr uint8_t TFT_CS = 7;
constexpr uint8_t TFT_RST = 2;
constexpr uint8_t TFT_DC = 3;
constexpr uint8_t DHT_PIN = 10;
constexpr uint8_t HUMID_PIN = 8;

// Temperature & humidity
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// Display
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Bluetooth Serial
BluetoothSerial SerialBT;

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

// Temperature & humidity
float currentTemp = 0;
float currentHumidity = 0;
unsigned long lastSensorRead = 0;
constexpr SENSOR_READ_INTERVAL = 2000;     // 2 seconds
constexpr DISPLAY_UPDATE_INTERVAL = 2000;  // 2 seconds

// RGB
uint8_t rgbR = 0, rgbG = 0, rgbB = 0;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("Smart-Humidifier");

  // Temperature & humidity
  dht.begin();

  // Humidifier
  pinMode(HUMID_PIN, OUTPUT);
  digitalWrite(HUMID_PIN, LOW);

  // Display
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  updateDisplay();
}

void loop() {
  unsigned long now = millis();

  // Bluetooth
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();
    processCommand(cmd);
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

void updateDisplay() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 10);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_CYAN);

  tft.print("Temp: ");
  tft.print(currentTemp, 1);
  tft.println(" C");
  tft.print("Humidity: ");
  tft.print(currentHumidity, 1);
  tft.println(" %");
  tft.print("Threshold: ");
  tft.print(humidityThreshold, 1);
  tft.println(" %");

  tft.print("Mode: ");
  if (currentMode == AUTONOMOUS) {
    tft.println("AUTO");
  } else {
    tft.println("TIMED");
    tft.print("Interval: ");
    tft.print(timedInterval);
    tft.println("s");
    tft.print("For: ");
    tft.print(timedDuration);
    tft.println("s");
  }

  tft.println();
  tft.setTextColor(isHumidifying ? ST77XX_GREEN : ST77XX_RED);
  tft.print("Status: ");
  tft.println(isHumidifying ? "ON" : "OFF");

  static bool anim = false;
  anim = !anim;
  if (anim)
    tft.fillCircle(150, 120, 3, ST77XX_YELLOW);
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
    resp = "Display on";
    updateDisplay();
  } else if (cmd == "DOFF") {
    displayOn = false;
    tft.fillScreen(ST77XX_BLACK);
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

  SerialBT.println(resp);
  Serial.println(cmd + " -> " + resp);
}