#pragma once

#include <Arduino.h>
#include <vector>
#include "helpers/error_codes.h"

class Sensor {
public:
    virtual ~Sensor() = default;
    
    // Read a specific measurement type from the sensor
    virtual float readValue(const String& measurementType) = 0;
    
    // Get list of supported measurement types for this sensor
    virtual std::vector<String> getSupportedMeasurements() const = 0;
    
    // Get the sensor type identifier (e.g., "hdc1080", "bme680")
    virtual String getSensorType() const = 0;
    
    // Check if sensor is properly initialized and working
    virtual bool isHealthy() const = 0;
    
protected:
    // Initialize the sensor hardware
    virtual bool begin() = 0;
};

// Base implementation class that provides common functionality
class BaseSensor : public Sensor {
public:
    BaseSensor() : initialized(false) {}
    
    float readValue(const String& measurementType) override {
        if (!initialized) {
            if (!begin()) {
                return ERROR_SENSOR_INIT_FAILED;
            }
            initialized = true;
        }
        
        std::vector<String> supported = getSupportedMeasurements();
        for (const String& measurement : supported) {
            if (measurement.equalsIgnoreCase(measurementType)) {
                return readMeasurement(measurementType);
            }
        }
        
        return ERROR_MEASUREMENT_NOT_SUPPORTED;
    }
    
    bool isHealthy() const override {
        return initialized;
    }
    
protected:
    bool initialized;
    
    // Sensor-specific measurement reading implementation
    virtual float readMeasurement(const String& measurementType) = 0;
};