#pragma once

#include <Adafruit_HDC1000.h>
#include "sensor.h"

class HDC1000Sensor : public Sensor {
public:
    HDC1000Sensor();
    float read() override;
private:
    bool begin() override;
    Adafruit_HDC1000 hdc;
    bool initialized;
};
