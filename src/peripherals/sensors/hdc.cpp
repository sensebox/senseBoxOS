#include "peripherals/sensors/hdc.h"
#include <Arduino.h>

// HDC1080 Sensor Implementation
HDC1080Sensor::HDC1080Sensor() : BaseSensor() {}

std::vector<String> HDC1080Sensor::getSupportedMeasurements() const {
    return {"temperature", "humidity"};
}

String HDC1080Sensor::getSensorType() const {
    return "hdc1080";
}

bool HDC1080Sensor::begin() {
    if (hdc.begin()) {
        Serial.println("HDC1080 Sensor initialized");
        return true;
    } else {
        Serial.println("HDC1080 Sensor not found!");
        return false;
    }
}

float HDC1080Sensor::readMeasurement(const String& measurementType) {
    float result = ERROR_UNKNOWN;
    
    try {
        if (measurementType.equalsIgnoreCase("temperature")) {
            result = hdc.readTemperature();
            
            // Validate temperature reading (HDC1080 range: -20°C to 85°C)
            if (result < -30.0f || result > 100.0f) {
                logError(ERROR_SENSOR_READ_FAILED, "HDC1080: Temperature reading out of expected range: " + String(result) + "°C");
                return ERROR_SENSOR_READ_FAILED;
            }
            
        } else if (measurementType.equalsIgnoreCase("humidity")) {
            result = hdc.readHumidity();
            
            // Validate humidity reading (HDC1080 range: 0% to 100% RH)
            if (result < 0.0f || result > 100.0f) {
                logError(ERROR_SENSOR_READ_FAILED, "HDC1080: Humidity reading out of expected range: " + String(result) + "%");
                return ERROR_SENSOR_READ_FAILED;
            }
            
        } else {
            // This should not happen due to validation in base class, but just in case
            logError(ERROR_MEASUREMENT_NOT_SUPPORTED, "HDC1080: Invalid measurement type: " + measurementType);
            return ERROR_MEASUREMENT_NOT_SUPPORTED;
        }
        
        // Check for NaN or infinite values
        if (isnan(result) || isinf(result)) {
            logError(ERROR_SENSOR_READ_FAILED, "HDC1080: Sensor returned invalid value (NaN or Inf) for " + measurementType);
            return ERROR_SENSOR_READ_FAILED;
        }
        
        return result;
        
    } catch (...) {
        logError(ERROR_COMMUNICATION_FAILED, "HDC1080: Exception occurred while reading " + measurementType);
        return ERROR_COMMUNICATION_FAILED;
    }
}
