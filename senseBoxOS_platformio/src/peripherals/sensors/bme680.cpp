#include "peripherals/sensors/bme680.h"
#include <Arduino.h>

int BME680Sensor::warmup_timeout_ms = 3000;

// BME680 Sensor Implementation
bsecSensor BME680Sensor::sensorList[14] = {
  BSEC_OUTPUT_IAQ,
  BSEC_OUTPUT_RAW_TEMPERATURE,
  BSEC_OUTPUT_RAW_PRESSURE,
  BSEC_OUTPUT_RAW_HUMIDITY,
  BSEC_OUTPUT_RAW_GAS,
  BSEC_OUTPUT_STABILIZATION_STATUS,
  BSEC_OUTPUT_RUN_IN_STATUS,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
  BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  BSEC_OUTPUT_STATIC_IAQ,
  BSEC_OUTPUT_CO2_EQUIVALENT,
  BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
  BSEC_OUTPUT_GAS_PERCENTAGE,
  BSEC_OUTPUT_COMPENSATED_GAS
};

BME680Sensor::BME680Sensor() : BaseSensor() {initialized = true;}

std::vector<String> BME680Sensor::getSupportedMeasurements() const {
    return {"temperature", "humidity", "pressure", "iaq", "co2eq"};
}

String BME680Sensor::getSensorType() const {
    return "bme680";
}

// as an alternative to "warming up" the sensor,
// we could also attach a callback to sensor, that triggers whenever new data is ready
bool BME680Sensor::warmUp(int timeout) {
    while (timeout > 0) {
        bme680.run();
        if (bme680.status == BSEC_OK && bme680.sensor.status == BME68X_OK) {
            return true;
        }
        delay(100);
        timeout -= 100;
    }
    // Check BSEC status for more specific error information
    if (bme680.status != BSEC_OK) {
        logError(ERROR_COMMUNICATION_FAILED, "BME680: BSEC error status " + String(bme680.status));
    } else if (bme680.sensor.status != BME68X_OK) {
        logError(ERROR_COMMUNICATION_FAILED, "BME680: Sensor error status " + String(bme680.sensor.status));
    } else {
        logError(ERROR_SENSOR_READ_FAILED, "BME680: No new data available");
    }
    return false;
}

bool BME680Sensor::begin() {
    Serial.println("[BME680] Attempting to initialize BME680...");
    bme680.begin(BME68X_I2C_ADDR_LOW, Wire);
    if (bme680.status != BSEC_OK || bme680.sensor.status != BME68X_OK) {
        Serial.println("[BME680] Sensor not detected - BME680 features disabled");
        sensorAvailable = false;
        return false;
    }

    bme680.updateSubscription(sensorList, ARRAY_LEN(sensorList), 1);
    if (!warmUp(warmup_timeout_ms)) {
        Serial.println("BME680 Sensor warm-up timed out!");
        return ERROR_SENSOR_READ_FAILED;
    }

    if (bme680.status != BSEC_OK || bme680.sensor.status != BME68X_OK) {
        Serial.println("[BME680] Sensor subscription failed!");
        sensorAvailable = false;
        return false;
    }
    // Check if IAQ and CO2eq outputs are available and valid
    float iaq = bme680.getData(BSEC_OUTPUT_IAQ).signal;
    float co2eq = bme680.getData(BSEC_OUTPUT_CO2_EQUIVALENT).signal;

    if (isnan(iaq) || iaq < 0.0f || iaq > 500.0f) {
        logError(ERROR_SENSOR_READ_FAILED, "BME680: IAQ reading out of expected range: " + String(iaq));
        sensorAvailable = false;
        return false;
    }

    if (isnan(co2eq) || co2eq < 250.0f || co2eq > 5000.0f) {
        logError(ERROR_SENSOR_READ_FAILED, "BME680: CO2eq reading out of expected range: " + String(co2eq));
        sensorAvailable = false;
        return false;
    }
    
    sensorAvailable = true;
    Serial.println("[BME680] Sensor detected and initialized");
    return true;
}

void BME680Sensor::updateSensorData() {
    // Skip if sensor not available
    if (!sensorAvailable) return;
    
    // Prüfe, ob genug Zeit seit dem letzten Update vergangen ist
    unsigned long currentTime = millis();
    if (currentTime - lastUpdateTime < updateInterval) {
        // Zu früh für ein Update, überspringe
        return;
    }
    
    lastUpdateTime = currentTime;
    
    // Rufe bme680.run() auf um neue Daten zu bekommen
    if (!bme680.run()) {
        Serial.println("BME680: bme680.run() returned false");
        return;
    }
    
    // Prüfe ob neue Daten verfügbar sind
    if (bme680.status != BSEC_OK) {
        Serial.println("BME680: BSEC status not OK: " + String(bme680.status));
        dataValid = false;
        return;
    }

    // Alle Sensorwerte aktualisieren
    cachedTemperature = bme680.getData(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE).signal - 7 ;
    cachedHumidity = bme680.getData(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY).signal;
    cachedPressure = bme680.getData(BSEC_OUTPUT_RAW_PRESSURE).signal;
    cachedIaq = bme680.getData(BSEC_OUTPUT_IAQ).signal;
    cachedCo2eq = bme680.getData(BSEC_OUTPUT_CO2_EQUIVALENT).signal;
    
    // Validierung der Werte
    dataValid = true;
    
    if (cachedTemperature < -50.0f || cachedTemperature > 100.0f) {
        logError(ERROR_SENSOR_READ_FAILED, "BME680: Temperature out of range: " + String(cachedTemperature));
        dataValid = false;
    }
    
    if (cachedHumidity < 0.0f || cachedHumidity > 100.0f) {
        logError(ERROR_SENSOR_READ_FAILED, "BME680: Humidity out of range: " + String(cachedHumidity));
        dataValid = false;
    }
    
    if (cachedPressure < 200.0f || cachedPressure > 1200.0f) {
        logError(ERROR_SENSOR_READ_FAILED, "BME680: Pressure out of range: " + String(cachedPressure));
        dataValid = false;
    }
    
    if (cachedIaq < 0.0f || cachedIaq > 500.0f) {
        logError(ERROR_SENSOR_READ_FAILED, "BME680: IAQ out of range: " + String(cachedIaq));
        dataValid = false;
    }
    
    if (cachedCo2eq < 250.0f || cachedCo2eq > 5000.0f) {
        logError(ERROR_SENSOR_READ_FAILED, "BME680: CO2eq out of range: " + String(cachedCo2eq));
        dataValid = false;
    }

}

float BME680Sensor::readMeasurement(const String& measurementType) {
    
    float result = ERROR_UNKNOWN;
    
    try {
        if (measurementType.equalsIgnoreCase("temperature")) {
            result = cachedTemperature;
            
        } else if (measurementType.equalsIgnoreCase("humidity")) {
            result = cachedHumidity;
            
        } else if (measurementType.equalsIgnoreCase("pressure")) {
            result = cachedPressure;
            
        } else if (measurementType.equalsIgnoreCase("iaq")) {
            result = cachedIaq;
            
        } else if (measurementType.equalsIgnoreCase("co2eq")) {
            result = cachedCo2eq;
            
        } else {
            logError(ERROR_MEASUREMENT_NOT_SUPPORTED, "BME680: Invalid measurement type: " + measurementType);
            return ERROR_MEASUREMENT_NOT_SUPPORTED;
        }
        
        // Check for NaN or infinite values
        if (isnan(result) || isinf(result)) {
            logError(ERROR_SENSOR_READ_FAILED, "BME680: Cached value is invalid (NaN or Inf) for " + measurementType);
            return ERROR_SENSOR_READ_FAILED;
        }
        
        return result;
        
    } catch (...) {
        logError(ERROR_COMMUNICATION_FAILED, "BME680: Exception occurred while reading " + measurementType);
        return ERROR_COMMUNICATION_FAILED;
    }
}
