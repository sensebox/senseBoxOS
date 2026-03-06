#include "peripherals/sensors/light.h"

LightSensor::LightSensor() : BaseSensor() {}

std::vector<String> LightSensor::getSupportedMeasurements() const {
    return {"light"};
}

String LightSensor::getSensorType() const {
	return "light";
}

bool LightSensor::begin() {
	return true;
}

float LightSensor::readMeasurement(const String& measurementType) {
	if (measurementType == "light") {
		return analogRead(PD_SENSE);
	}
	return ERROR_MEASUREMENT_NOT_SUPPORTED;
}
