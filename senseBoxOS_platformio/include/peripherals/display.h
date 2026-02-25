#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
extern Adafruit_SSD1306 oled;
extern bool oledInitialized;

void initDisplay();
void  displayNumber(float value);
void clearDisplay();
void handleDisplay(String args);
void displayText(const String& text);
void resetDisplayTextY();
void handleClearDisplay(String args);
String getDeviceID();
void displayDeviceID();

// Global device ID
extern String deviceID;
