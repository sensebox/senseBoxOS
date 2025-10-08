#pragma once

#include <bsec2.h>
#include "sensor.h"

class BME680Sensor : public BaseSensor {
public:
    BME680Sensor();
    
    std::vector<String> getSupportedMeasurements() const override;
    String getSensorType() const override;
    
protected:
    bool begin() override;
    float readMeasurement(const String& measurementType) override;
    
private:
    Bsec2 bme680;
    static bsecSensor sensorList[14];
};
