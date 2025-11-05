#pragma once

#include <Adafruit_HDC1000.h>
#include "sensor.h"

class HDC1080Sensor : public BaseSensor {
public:
    HDC1080Sensor();
    
    std::vector<String> getSupportedMeasurements() const override;
    String getSensorType() const override;
    
protected:
    bool begin() override;
    float readMeasurement(const String& measurementType) override;
    
private:
    Adafruit_HDC1000 hdc;
};
