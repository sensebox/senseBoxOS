#include "helpers/sensor_registry.h"
#include "helpers/error_codes.h"
#include "peripherals/sensors/hdc.h"
#include "peripherals/sensors/bme680.h"
#include "peripherals/sensors/accelerometer.h"
#include "peripherals/sensors/light.h"

SensorRegistry sensorRegistry;

float SensorRegistry::readSensor(const String& sensorType, const String& measurement) {
    // Initialize sensor on-demand
    auto it = sensors.find(sensorType);
    if (it == sensors.end()) {
        Sensor* sensor = nullptr;
        
        if (sensorType == "hdc1080") {
            sensor = new HDC1080Sensor();
        } else if (sensorType == "bme680") {
            sensor = new BME680Sensor();
        } else if (sensorType == "accelerometer") {
            sensor = new AccelerometerSensor();
        } else if (sensorType == "board") {
            sensor = new LightSensor();
        }
        
        if (sensor == nullptr) {
            return ERROR_SENSOR_NOT_FOUND;
        }
        
        sensors[sensorType] = sensor;
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