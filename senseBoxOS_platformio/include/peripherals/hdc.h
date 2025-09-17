#pragma once

#include <Adafruit_HDC1000.h>

extern Adafruit_HDC1000 hdc;
extern bool hdcInitialized;

float readSensor();
