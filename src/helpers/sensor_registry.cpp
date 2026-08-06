#include "helpers/sensor_registry.h"
#include "helpers/error_codes.h"
#include "peripherals/sensors/hdc.h"
#include "peripherals/sensors/bme680.h"
#include "peripherals/sensors/accelerometer.h"
#include "peripherals/sensors/light.h"

SensorRegistry sensorRegistry;

void SensorRegistry::registerSensor(const String& sensorType, Sensor* sensor) {
    if (sensor != nullptr) {
        sensors[sensorType] = sensor;
    }
}

float SensorRegistry::readSensor(const String& sensorType, const String& measurement) {
    // Handle automatic sensor selection for temperature/humidity
    // Users can request "sensor:temperature:temperature", "sensor:humidity:humidity", 
    // "sensor:auto:temperature", "sensor:generic:humidity", etc.
    String actualSensorType = sensorType;
    
    if (sensorType.equalsIgnoreCase("auto") || 
        sensorType.equalsIgnoreCase("generic") ||
        sensorType.equalsIgnoreCase("temperature") ||
        sensorType.equalsIgnoreCase("humidity")) {
        
        // For auto-selection, prefer available sensor with the requested measurement
        if (measurement.equalsIgnoreCase("temperature") || measurement.equalsIgnoreCase("humidity")) {
            // Try BME680 first (has more features), then HDC1080
            if (sensors.find("bme680") != sensors.end()) {
                actualSensorType = "bme680";
            } else if (sensors.find("hdc1080") != sensors.end()) {
                actualSensorType = "hdc1080";
            } else {
                Serial.printf("[SensorRegistry] No temperature/humidity sensor available\n");
                return ERROR_SENSOR_NOT_FOUND;
            }
        }
    }
    
    // Check if sensor is registered
    auto it = sensors.find(actualSensorType);
    if (it == sensors.end()) {
        // Sensor not found in registry
        Serial.printf("[SensorRegistry] Sensor '%s' not registered\n", actualSensorType.c_str());
        
        // Try automatic fallback for temperature/humidity between BME680 and HDC1080
        if (measurement.equalsIgnoreCase("temperature") || measurement.equalsIgnoreCase("humidity")) {
            String fallbackSensor = "";
            
            if (actualSensorType.equalsIgnoreCase("bme680") && sensors.find("hdc1080") != sensors.end()) {
                fallbackSensor = "hdc1080";
                Serial.printf("[SensorRegistry] BME680 not available, trying fallback HDC1080 for %s\n", measurement.c_str());
            } else if (actualSensorType.equalsIgnoreCase("hdc1080") && sensors.find("bme680") != sensors.end()) {
                fallbackSensor = "bme680";
                Serial.printf("[SensorRegistry] HDC1080 not available, trying fallback BME680 for %s\n", measurement.c_str());
            }
            
            if (fallbackSensor.length() > 0) {
                return sensors[fallbackSensor]->readValue(measurement);
            }
        }
        
        return ERROR_SENSOR_NOT_FOUND;
    }
    
    return sensors[actualSensorType]->readValue(measurement);
}

bool SensorRegistry::hasSensor(const String& sensorType) const {
    return sensors.find(sensorType) != sensors.end();
}

std::vector<String> SensorRegistry::getRegisteredSensors() const {
    std::vector<String> sensorTypes;
    sensorTypes.reserve(sensors.size());
    
    for (const auto& pair : sensors) {
        sensorTypes.push_back(pair.first);
    }
    
    return sensorTypes;
}

std::vector<String> SensorRegistry::getSupportedMeasurements(const String& sensorType) const {
    if (sensorType.length() == 0) {
        logError(ERROR_EMPTY_PARAMETER, "getSupportedMeasurements: Sensor type is empty");
        return std::vector<String>();
    }
    
    auto it = sensors.find(sensorType);
    if (it == sensors.end()) {
        logError(ERROR_SENSOR_NOT_FOUND, "getSupportedMeasurements: Unknown sensor type '" + sensorType + "'");
        return std::vector<String>();
    }
    
    Sensor* sensor = it->second;
    if (sensor == nullptr) {
        logError(ERROR_NULL_POINTER, "getSupportedMeasurements: Null sensor instance for type '" + sensorType + "'");
        return std::vector<String>();
    }
    
    return sensor->getSupportedMeasurements();
}

void SensorRegistry::pollSensors() {
    static unsigned long lastDebug = 0;
    unsigned long now = millis();
    if (now - lastDebug > 3000) {
        lastDebug = now;
    }
    
    for (auto &pair : sensors) {
        Sensor* s = pair.second;
        if (s != nullptr) {
            s->poll();
        }
    }
}