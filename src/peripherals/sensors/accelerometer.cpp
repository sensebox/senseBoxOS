#include "peripherals/sensors/accelerometer.h"
#include <Arduino.h>

AccelerometerSensor::AccelerometerSensor() 
    : BaseSensor(), 
      icm(Wire1, 0),
      activeSensor(ACCEL_NONE),
      shakeThreshold(3.0),  // m/s² - adjust for sensitivity (lowered for easier detection)
      lastShakeDetectedTime(0),
      shakeHoldTime(1000)  // ms - keep shake active for 1.5 seconds after detection
{
}

std::vector<String> AccelerometerSensor::getSupportedMeasurements() const {
    return {"accelerationX", "accelerationY", "accelerationZ", "temperature"};
}

String AccelerometerSensor::getSensorType() const {
    return "accelerometer";
}

bool AccelerometerSensor::begin() {
    Wire1.begin();
    
    // Try MPU6050 first
    if (mpu.begin(0x68, &Wire1)) {
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        activeSensor = ACCEL_MPU6050;
        Serial.println("MPU6050 Accelerometer initialized");
        return true;
    }
    
    // If MPU6050 fails, try ICM42670P
    if (icm.begin() == 0) {
        icm.startAccel(21, 8); // Accel ODR = 100 Hz, Range = 8G
        activeSensor = ACCEL_ICM42670P;
        Serial.println("ICM42670P Accelerometer initialized");
        return true;
    }
    
    Serial.println("No accelerometer sensor found!");
    activeSensor = ACCEL_NONE;
    return false;
}

float AccelerometerSensor::readMeasurement(const String& measurementType) {
    if (activeSensor == ACCEL_NONE) {
        logError(ERROR_SENSOR_INIT_FAILED, "Accelerometer: No sensor initialized");
        return ERROR_SENSOR_INIT_FAILED;
    }
    
    float result = ERROR_UNKNOWN;
    
    try {
        if (measurementType.equalsIgnoreCase("accelerationX")) {
            result = getAccelerationX();
        } else if (measurementType.equalsIgnoreCase("accelerationY")) {
            result = getAccelerationY();
        } else if (measurementType.equalsIgnoreCase("accelerationZ")) {
            result = getAccelerationZ();
        } else if (measurementType.equalsIgnoreCase("temperature")) {
            result = getTemperature();
        } else {
            logError(ERROR_MEASUREMENT_NOT_SUPPORTED, "Accelerometer: Invalid measurement type: " + measurementType);
            return ERROR_MEASUREMENT_NOT_SUPPORTED;
        }
        
        // Check for NaN or infinite values
        if (isnan(result) || isinf(result)) {
            logError(ERROR_SENSOR_READ_FAILED, "Accelerometer: Sensor returned invalid value (NaN or Inf) for " + measurementType);
            return ERROR_SENSOR_READ_FAILED;
        }
        
        return result;
        
    } catch (...) {
        logError(ERROR_COMMUNICATION_FAILED, "Accelerometer: Exception occurred while reading " + measurementType);
        return ERROR_COMMUNICATION_FAILED;
    }
}

float AccelerometerSensor::getAccelerationX() {
    if (activeSensor == ACCEL_MPU6050) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        return a.acceleration.x;
    } else if (activeSensor == ACCEL_ICM42670P) {
        inv_imu_sensor_event_t imu_event;
        icm.getDataFromRegisters(imu_event);
        return (imu_event.accel[0] * 9.81) / 4096.0;
    }
    return 0.0;
}

float AccelerometerSensor::getAccelerationY() {
    if (activeSensor == ACCEL_MPU6050) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        return a.acceleration.y;
    } else if (activeSensor == ACCEL_ICM42670P) {
        inv_imu_sensor_event_t imu_event;
        icm.getDataFromRegisters(imu_event);
        return (imu_event.accel[1] * 9.81) / 4096.0;
    }
    return 0.0;
}

float AccelerometerSensor::getAccelerationZ() {
    if (activeSensor == ACCEL_MPU6050) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        return a.acceleration.z;
    } else if (activeSensor == ACCEL_ICM42670P) {
        inv_imu_sensor_event_t imu_event;
        icm.getDataFromRegisters(imu_event);
        return (imu_event.accel[2] * 9.81) / 4096.0;
    }
    return 0.0;
}

float AccelerometerSensor::getTemperature() {
    if (activeSensor == ACCEL_MPU6050) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        return temp.temperature;
    } else if (activeSensor == ACCEL_ICM42670P) {
        inv_imu_sensor_event_t imu_event;
        icm.getDataFromRegisters(imu_event);
        return (imu_event.temperature / 132.48) + 25.0;
    }
    return 0.0;
}

float AccelerometerSensor::calculateMagnitude(float x, float y, float z) {
    return sqrt(x * x + y * y + z * z);
}

bool AccelerometerSensor::isShaken() {
    // Track last state to only log on changes
    static bool lastShakeState = false;
    
    if (!initialized) {
        Serial.println("[Shake] Initializing accelerometer...");
        if (!begin()) {
            Serial.println("[Shake] ERROR: Failed to initialize accelerometer!");
            return false;
        }
        initialized = true;
        Serial.print("[Shake] Initialized with sensor type: ");
        Serial.println(activeSensor);
    }
    
    if (activeSensor == ACCEL_NONE) {
        Serial.println("[Shake] No sensor active!");
        return false;
    }
    
    // Get current acceleration values
    float x = getAccelerationX();
    float y = getAccelerationY();
    float z = getAccelerationZ();
    
    // Calculate total acceleration magnitude
    float magnitude = calculateMagnitude(x, y, z);
    
    // Subtract gravity (9.81 m/s²) to get net acceleration
    float netAccel = abs(magnitude - 9.81);
    
    // Check if shake threshold exceeded right now
    bool shakeDetectedNow = netAccel > shakeThreshold;
    
    // Update last shake time if shake is detected
    unsigned long currentTime = millis();
    if (shakeDetectedNow) {
        lastShakeDetectedTime = currentTime;
    }
    
    // Keep shake state true for shakeHoldTime ms after last detection
    bool isCurrentlyShaken = (currentTime - lastShakeDetectedTime) < shakeHoldTime;
    
    // Only log when state changes
    if (lastShakeState != isCurrentlyShaken) {
        lastShakeState = isCurrentlyShaken;
        if (isCurrentlyShaken) {
            Serial.print("[Shake] DETECTED! Net acceleration: ");
            Serial.println(netAccel);
        } else {
            Serial.println("[Shake] Ended (hold time expired)");
        }
    }
    
    return isCurrentlyShaken;
}
