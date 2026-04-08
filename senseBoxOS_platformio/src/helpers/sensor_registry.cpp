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
    // Check if sensor is already registered
    auto it = sensors.find(sensorType);
    if (it == sensors.end()) {
        // Sensor not found in registry
        Serial.printf("[SensorRegistry] Sensor '%s' not registered\n", sensorType.c_str());
        return ERROR_SENSOR_NOT_FOUND;
    }
    
    return sensors[sensorType]->readValue(measurement);
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
        Serial.printf("[Registry] Polling %d sensors\n", sensors.size());
        lastDebug = now;
    }
    
    for (auto &pair : sensors) {
        Sensor* s = pair.second;
        if (s != nullptr) {
            s->poll();
        }
    }
}