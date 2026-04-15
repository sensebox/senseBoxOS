#pragma once

#ifndef SENSEBOX_OS_VERSION
  #define SENSEBOX_OS_VERSION "0.0.0"
#endif
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
extern Adafruit_SSD1306 oled;
extern bool oledInitialized;

void initDisplay();
void  displayNumber(float value, uint8_t textSize = 1, const String& unit = "");
void clearDisplay();
void handleDisplay(String args);
void displayText(const String& text, uint8_t textSize = 1);
void resetDisplayTextY();
void handleClearDisplay(String args);
String getDeviceID();
void displayDeviceID();
void displaySerialOnlyMode();
void displayMeasurement(float value, const String& sensorName, const String& unit, int decimals = 2);
void handleDisplayMeasurement(String args);

// Global device ID
extern String deviceID;
