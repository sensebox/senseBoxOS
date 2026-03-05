#pragma once

#include <bsec2.h>
#include "sensor.h"

class BME680Sensor : public BaseSensor {
public:
    BME680Sensor();
    
    std::vector<String> getSupportedMeasurements() const override;
    String getSensorType() const override;
    bool begin() override;
    bool warmUp(int timeout);
    void updateSensorData();
    float readMeasurement(const String& measurementType) override;


    
protected:
    
private:
    static int warmup_timeout_ms;
    static bsecSensor sensorList[14];
    Bsec2 bme680;
    
    // Globale Variablen für kontinuierlich gepollt Sensorwerte
    float cachedTemperature = 0.0f;
    float cachedHumidity = 0.0f;
    float cachedPressure = 0.0f;
    float cachedIaq = 0.0f;
    float cachedCo2eq = 0.0f;
    bool dataValid = false;
    
    // Zeitsteuerung für Updates
    unsigned long lastUpdateTime = 0;
    static const unsigned long updateInterval = 1000; // 3 Sekunden zwischen Updates
};
