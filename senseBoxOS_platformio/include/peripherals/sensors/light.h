
#pragma once
#include "sensor.h"

class LightSensor : public BaseSensor {
public:
    LightSensor();
    std::vector<String> getSupportedMeasurements() const override;
    String getSensorType() const override;

protected:
    bool begin() override;
    float readMeasurement(const String& measurementType) override;
};
