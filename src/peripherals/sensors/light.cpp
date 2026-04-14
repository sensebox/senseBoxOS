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
		float value = analogRead(PD_SENSE);
		if (value < 30) return 0.0f;
		// Map [30, 4095] -> [1, 1000]
		float mapped = (value - 30.0f) / (4095.0f - 30.0f) * 999.0f + 1.0f;
		return mapped;
	}
	return ERROR_MEASUREMENT_NOT_SUPPORTED;
}
