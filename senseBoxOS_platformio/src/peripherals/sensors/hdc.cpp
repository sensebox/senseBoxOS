#include "peripherals/sensors/hdc.h"
#include <Arduino.h>

HDC1000Sensor::HDC1000Sensor() : initialized(false) {}

bool HDC1000Sensor::begin() {
    if (!initialized) {
        if (hdc.begin()) {
            initialized = true;
            Serial.println("HDC1000 initialized");
        } else {
            Serial.println("HDC1000 not found!");
            return false;
        }
    }
    return true;
}

float HDC1000Sensor::read() {
    if (!initialized) {
        if (!begin()) return -999;
    }
    return hdc.readTemperature();
}
