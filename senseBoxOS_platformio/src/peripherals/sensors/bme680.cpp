#include "peripherals/sensors/bme680.h"
#include <Arduino.h>

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

BME680Sensor::BME680Sensor() : initialized(false) {}

bool BME680Sensor::begin() {
  bme680.begin(BME68X_I2C_ADDR_LOW, Wire);
  if (bme680.status != BSEC_OK || bme680.sensor.status != BME68X_OK) {
    return false;
  }

  bme680.updateSubscription(sensorList, ARRAY_LEN(sensorList), BSEC_SAMPLE_RATE_ULP);
  if (bme680.status != BSEC_OK || bme680.sensor.status != BME68X_OK) {
    return false;
  }
  return true;
}

float BME680Sensor::read() {
  if (!initialized) {
    if (!begin()) return -999;
    initialized = true;
  }
  if (bme680.run()) {
    return bme680.getData(BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE).signal;
  } else {
    return -999; // error or no new data
  }
}
