#pragma once

#include <Adafruit_NeoPixel.h>

extern Adafruit_NeoPixel rgb_led_1;

void initLedRGB();
void setLedRGB(int r, int g, int b);
void randomLed();
void handleLed(String args);
void handleRandomLed(String args);
