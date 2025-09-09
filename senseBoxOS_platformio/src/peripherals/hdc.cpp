#include "peripherals/hdc.h"

Adafruit_HDC1000 hdc;
bool hdcInitialized = false;

float readSensor() {
  if (!hdcInitialized) {
    if (hdc.begin()) {
      hdcInitialized = true;
      Serial.println("HDC1000 initialized");
    } else {
      Serial.println("HDC1000 not found!");
      return -999; // error
    }
  }
  return hdc.readTemperature();
}