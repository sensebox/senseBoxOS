#pragma once

#include <bsec2.h>
#include "sensor.h"

class BME680Sensor : public Sensor {
public:
    BME680Sensor();
    float read() override;
private:
    bool begin() override;
    Bsec2 bme680;
    bool initialized;
};
