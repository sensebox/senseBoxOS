#pragma once

#include <Adafruit_MPU6050.h>
#include "ICM42670P.h"
#include <Adafruit_ICM20948.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "sensor.h"

enum AccelSensorType {
    ACCEL_NONE = 0,
    ACCEL_MPU6050 = 1,
    ACCEL_ICM42670P = 2,
    ACCEL_ICM20948 = 3
};

class AccelerometerSensor : public BaseSensor {
public:
    AccelerometerSensor();
    
    std::vector<String> getSupportedMeasurements() const override;
    String getSensorType() const override;
    
    // Check if box is being shaken
    bool isShaken();
    
protected:
    bool begin() override;
    float readMeasurement(const String& measurementType) override;
    
private:
    Adafruit_MPU6050 mpu;
    ICM42670 icm;
    Adafruit_ICM20948 icm2;

    AccelSensorType activeSensor;
    
    // Shake detection parameters
    float shakeThreshold;
    unsigned long lastShakeDetectedTime;
    unsigned long shakeHoldTime;  // How long to keep shake state true after detection
    
    // Helper methods
    float getAccelerationX();
    float getAccelerationY();
    float getAccelerationZ();
    float getTemperature();
    float calculateMagnitude(float x, float y, float z);
};
