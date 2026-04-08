
#pragma once
#include "sensor.h"

// Light sensor pin definition for senseBox MCU ESP32-S2
#ifndef PD_SENSE
#define PD_SENSE 1  // GPIO1 for light sensor on senseBox MCU
#endif

class LightSensor : public BaseSensor {
public:
    LightSensor();
    std::vector<String> getSupportedMeasurements() const override;
    String getSensorType() const override;
    bool begin() override;

protected:
    float readMeasurement(const String& measurementType) override;
};
