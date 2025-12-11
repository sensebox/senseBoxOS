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

BME680Sensor::BME680Sensor() : BaseSensor() {}

std::vector<String> BME680Sensor::getSupportedMeasurements() const {
    return {"temperature", "humidity", "pressure"};
}

String BME680Sensor::getSensorType() const {
    return "bme680";
}

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
    bme680.begin(BME68X_I2C_ADDR_LOW, Wire);
    if (bme680.status != BSEC_OK || bme680.sensor.status != BME68X_OK) {
        Serial.println("BME680 Sensor initialization failed!");
        return false;
    }

    bme680.updateSubscription(sensorList, ARRAY_LEN(sensorList), 1);
    if (!warmUp(warmup_timeout_ms)) {
        Serial.println("BME680 Sensor warm-up timed out!");
        return ERROR_SENSOR_READ_FAILED;
    }

    if (bme680.status != BSEC_OK || bme680.sensor.status != BME68X_OK) {
        Serial.println("BME680 Sensor subscription failed!");
        return false;
    }
    
    Serial.println("BME680 Sensor initialized");
    return true;
}



float BME680Sensor::readMeasurement(const String& measurementType) {
    if (!warmUp(warmup_timeout_ms)) {
        Serial.println("BME680 Sensor warm-up timed out!");
        return ERROR_SENSOR_READ_FAILED;
    }
    
    float result = ERROR_UNKNOWN;
    
    try {
        if (measurementType.equalsIgnoreCase("temperature")) {
            result = bme680.getData(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE).signal;
            
            // Validate temperature reading (BME680 range: -40°C to 85°C)
            if (result < -50.0f || result > 100.0f) {
                logError(ERROR_SENSOR_READ_FAILED, "BME680: Temperature reading out of expected range: " + String(result) + "°C");
                return ERROR_SENSOR_READ_FAILED;
            }
            
        } else if (measurementType.equalsIgnoreCase("humidity")) {
            result = bme680.getData(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY).signal;
            
            // Validate humidity reading (BME680 range: 0% to 100% RH)
            if (result < 0.0f || result > 100.0f) {
                logError(ERROR_SENSOR_READ_FAILED, "BME680: Humidity reading out of expected range: " + String(result) + "%");
                return ERROR_SENSOR_READ_FAILED;
            }
            
        } else if (measurementType.equalsIgnoreCase("pressure")) {
            result = bme680.getData(BSEC_OUTPUT_RAW_PRESSURE).signal;
            
            // Validate pressure reading (BME680 range: 300-1100 hPa)
            if (result < 200.0f || result > 1200.0f) {
                logError(ERROR_SENSOR_READ_FAILED, "BME680: Pressure reading out of expected range: " + String(result) + " hPa");
                return ERROR_SENSOR_READ_FAILED;
            }
            
        } else {
            // This should not happen due to validation in base class, but just in case
            logError(ERROR_MEASUREMENT_NOT_SUPPORTED, "BME680: Invalid measurement type: " + measurementType);
            return ERROR_MEASUREMENT_NOT_SUPPORTED;
        }
        
        // Check for NaN or infinite values
        if (isnan(result) || isinf(result)) {
            logError(ERROR_SENSOR_READ_FAILED, "BME680: Sensor returned invalid value (NaN or Inf) for " + measurementType);
            return ERROR_SENSOR_READ_FAILED;
        }
        
        return result;
        
    } catch (...) {
        logError(ERROR_COMMUNICATION_FAILED, "BME680: Exception occurred while reading " + measurementType);
        return ERROR_COMMUNICATION_FAILED;
    }
}
