#pragma once

#include <Arduino.h>

// Pins - ESP32-C3
constexpr uint8_t TFT_CS = 7;
constexpr uint8_t TFT_RST = 2;
constexpr uint8_t TFT_DC = 3;
constexpr uint8_t TFT_LED = 8;
constexpr uint8_t DHT_PIN = 9;
constexpr uint8_t HUMID_PIN = 5;
constexpr uint8_t BUTTON_PIN = 1;

// RTC Memory Structure
RTC_DATA_ATTR struct {
  uint32_t bootCount = 0;
  bool wasSleeping = false;
  uint8_t savedMode = 0;
  float savedHumidityThreshold = 50.0;
  uint32_t savedTimedInterval = 3600;
  uint32_t savedTimedDuration = 300;
  uint32_t lastSensorReadTime = 0;
  uint32_t lastTimedStartTime = 0;
  uint32_t humidifyStartTime = 0;
  bool wasHumidifying = false;
  uint8_t displayState = 1;
  uint32_t displaySleepStart = 0;
  float lastTemp = 20.0;
  float lastHumidity = 50.0;
} rtcState;

// Temperature & humidity
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// Display
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// BLE
String bleDeviceName = "Smart-humidifier";
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
BLECharacteristic *pCharacteristic = NULL;
String receivedCommand = "";
bool bleInitialized = false;

// BLE Advertising
bool isAdvertising = false;
unsigned long advertisingStartTime = 0;
constexpr unsigned long ADVERTISING_DURATION = 120000;  // 2 minutes

// BLE Server & Advertising pointers
BLEServer *pServer = NULL;
BLEAdvertising *pAdvertising = NULL;

// Button
ezButton button(BUTTON_PIN);

class SmartHumidifierCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue().c_str();
    if (value.length() > 0) {
      receivedCommand = value;
      receivedCommand.trim();
    }
  }
};

// Global callback instance => prevent heap issues
SmartHumidifierCallbacks *pCallbacks = NULL;

// Operating modes
enum Mode {
  AUTONOMOUS,
  TIMED
};
Mode currentMode = AUTONOMOUS;

// Display states
enum DisplayState {
  DISPLAY_OFF,
  DISPLAY_ON,
  DISPLAY_SLEEPING
};
DisplayState displayState = DISPLAY_ON;

// Display timing
unsigned long lastDisplayUpdate = 0;
unsigned long lastDisplayWake = 0;
unsigned long displaySleepStartTime = 0;
constexpr unsigned long DISPLAY_UPDATE_INTERVAL = 2000;
constexpr unsigned long DISPLAY_WAKE_INTERVAL = 1800000;  // 30 minutes
constexpr unsigned long DISPLAY_WAKE_DURATION = 120000;   // 2 minutes

// Autonomous mode
float humidityThreshold = 50.0;
float hysteresis = 5.0;
bool isHumidifying = false;
unsigned long humidifyStartTime = 0;
constexpr unsigned long MIN_RUNTIME = 300000;  // 5 minutes

// Timed mode
unsigned long timedInterval = 3600;
unsigned long timedDuration = 300;
unsigned long lastTimedStart = 0;
bool timedModeFirstCycle = true;

// Temperature & humidity
float currentTemp = 0;
float currentHumidity = 0;
unsigned long lastSensorRead = 0;
constexpr unsigned long SENSOR_READ_INTERVAL = 30000;  // 30 seconds

// Previous display values
float prevTemp = -999;
float prevHumidity = -999;
float prevThreshold = -999;
Mode prevMode = AUTONOMOUS;
bool displayInitialized = false;

// Power Management
unsigned long lastActivityTime = 0;