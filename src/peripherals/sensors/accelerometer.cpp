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

// WHO_AM_I register/value constants (all sensors live on I2C address 0x68)
#define ACCEL_I2C_ADDRESS      0x68
#define ICM20948_WHOAMI_REG    0x00
#define ICM20948_WHOAMI_VAL    0xEA
#define IMU_WHOAMI_REG         0x75  // shared by MPU6050 and ICM42670P
#define MPU6050_WHOAMI_VAL     0x68
#define ICM42670P_WHOAMI_VAL   0x67

bool AccelerometerSensor::i2cDevicePresent(uint8_t address) {
    Wire1.beginTransmission(address);
    return (Wire1.endTransmission() == 0);
}

bool AccelerometerSensor::readRegister8(uint8_t address, uint8_t reg, uint8_t& value) {
    Wire1.beginTransmission(address);
    Wire1.write(reg);
    if (Wire1.endTransmission(false) != 0) {
        return false;
    }
    if (Wire1.requestFrom((int)address, 1) != 1) {
        return false;
    }
    value = Wire1.read();
    return true;
}

AccelSensorType AccelerometerSensor::identifySensor() {
    // Make sure a device actually ACKs before reading registers (avoids bus hang)
    if (!i2cDevicePresent(ACCEL_I2C_ADDRESS)) {
        Serial.println("[Accelerometer] No I2C device at 0x68");
        return ACCEL_NONE;
    }

    uint8_t whoami = 0;

    // Dump first few registers for debugging
    Serial.println("[Accelerometer] Diagnostic register dump at 0x68:");
    for (uint8_t reg = 0x00; reg <= 0x10; reg += 0x05) {
        uint8_t val;
        if (readRegister8(ACCEL_I2C_ADDRESS, reg, val)) {
            Serial.printf("[Accelerometer]   Reg 0x%02X = 0x%02X\n", reg, val);
        } else {
            Serial.printf("[Accelerometer]   Reg 0x%02X = READ FAILED\n", reg);
        }
    }

    // ICM20948 uses register 0x00 for WHO_AM_I (but may need bank selection)
    if (readRegister8(ACCEL_I2C_ADDRESS, ICM20948_WHOAMI_REG, whoami)) {
        Serial.printf("[Accelerometer] WHO_AM_I(0x00) = 0x%02X\n", whoami);
        // ICM20948 valid WHO_AM_I values: 0xEA (standard) or could be read from different bank
        if (whoami == ICM20948_WHOAMI_VAL || whoami == 0xE0 || whoami == 0xE1) {
            Serial.println("[Accelerometer] Detected as ICM20948 (0x00 register)");
            return ACCEL_ICM20948;
        }
    }

    // MPU6050 and ICM42670P use register 0x75 for WHO_AM_I
    if (readRegister8(ACCEL_I2C_ADDRESS, IMU_WHOAMI_REG, whoami)) {
        Serial.printf("[Accelerometer] WHO_AM_I(0x75) = 0x%02X\n", whoami);
        if (whoami == ICM42670P_WHOAMI_VAL) {
            Serial.println("[Accelerometer] Detected as ICM42670P (0x75 register)");
            return ACCEL_ICM42670P;
        }
        // MPU6050 standard is 0x68, but hardware variants may return different values
        // Accept 0x68 (standard), 0x1F, 0x21, or other MPU-family values
        if (whoami == MPU6050_WHOAMI_VAL || whoami == 0x1F || whoami == 0x21) {
            Serial.println("[Accelerometer] Detected as MPU6050 (0x75 register)");
            return ACCEL_MPU6050;
        }
    }

    Serial.println("[Accelerometer] Unknown WHO_AM_I response - possible alternate ICM variant or custom config");
    return ACCEL_NONE;
}

bool AccelerometerSensor::begin() {
    // Wire1 should already be initialized by main.cpp
    Serial.println("[Accelerometer] Attempting to initialize sensors...");
    delay(100);  // Small delay to allow I2C bus to settle

    // Identify the connected chip by its WHO_AM_I register before calling any
    // library begin(). This avoids the library probing hanging the I2C bus when
    // the wrong sensor sits on the shared 0x68 address.
    AccelSensorType detected = identifySensor();

    switch (detected) {
        case ACCEL_ICM20948:
            Serial.println("[Accelerometer] ICM20948 identified, initializing...");
            if (icm2.begin_I2C(ACCEL_I2C_ADDRESS, &Wire1)) {
                icm2.setAccelRange(ICM20948_ACCEL_RANGE_8_G);
                icm2.setAccelRateDivisor(10);
                activeSensor = ACCEL_ICM20948;
                initialized = true;
                Serial.println("[Accelerometer] ICM20948 detected and initialized");
                return true;
            }
            Serial.println("[Accelerometer] ICM20948 init failed after identification");
            break;

        case ACCEL_ICM42670P:
            Serial.println("[Accelerometer] ICM42670P identified, initializing...");
            if (icm.begin() == 0) {
                icm.startAccel(21, 8); // Accel ODR = 100 Hz, Range = 8G
                activeSensor = ACCEL_ICM42670P;
                initialized = true;
                Serial.println("[Accelerometer] ICM42670P detected and initialized");
                return true;
            }
            Serial.println("[Accelerometer] ICM42670P init failed after identification");
            break;

        case ACCEL_MPU6050:
            Serial.println("[Accelerometer] MPU6050 identified, initializing...");
            if (mpu.begin(ACCEL_I2C_ADDRESS, &Wire1)) {
                mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
                mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
                activeSensor = ACCEL_MPU6050;
                initialized = true;
                Serial.println("[Accelerometer] MPU6050 detected and initialized");
                return true;
            }
            Serial.println("[Accelerometer] MPU6050 init failed after identification");
            break;

        case ACCEL_NONE:
        default:
            break;
    }

    Serial.println("[Accelerometer] No accelerometer sensor found!");
    activeSensor = ACCEL_NONE;
    initialized = false;
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
    } else if (activeSensor == ACCEL_ICM20948) {
        sensors_event_t a, g, m, temp;
        icm2.getEvent(&a, &g, &m, &temp);
        return a.acceleration.x;
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
    } else if (activeSensor == ACCEL_ICM20948) {
        sensors_event_t a, g, m, temp;
        icm2.getEvent(&a, &g, &m, &temp);
        return a.acceleration.y;
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
    } else if (activeSensor == ACCEL_ICM20948) {
        sensors_event_t a, g, m, temp;
        icm2.getEvent(&a, &g, &m, &temp);
        return a.acceleration.z;
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
    } else if (activeSensor == ACCEL_ICM20948) {
        sensors_event_t a, g, m, temp;
        icm2.getEvent(&a, &g, &m, &temp);
        return temp.temperature;
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
