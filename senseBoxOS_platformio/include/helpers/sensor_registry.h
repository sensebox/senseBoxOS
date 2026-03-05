#pragma once

#include <Arduino.h>
#include <map>
#include "peripherals/sensors/sensor.h"

class SensorRegistry {
private:
    std::map<String, Sensor*> sensors;

public:
    SensorRegistry() = default;
    ~SensorRegistry() = default;
    
    // Register an external sensor instance
    void registerSensor(const String& sensorType, Sensor* sensor);
    
    // Read a value from a sensor (creates sensor on-demand)
    float readSensor(const String& sensorType, const String& measurement);
    
    // Check if a sensor type is registered
    bool hasSensor(const String& sensorType) const;
    
    // Get a list of all registered sensor types
    std::vector<String> getRegisteredSensors() const;
    
    // Get supported measurements for a specific sensor
    std::vector<String> getSupportedMeasurements(const String& sensorType) const;
};

extern SensorRegistry sensorRegistry;
