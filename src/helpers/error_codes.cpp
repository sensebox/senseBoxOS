#include "helpers/error_codes.h"
#include <Arduino.h>

String getErrorMessage(float errorCode) {
    switch ((int)errorCode) {
        case (int)ERROR_SUCCESS:
            return "Success";
        case (int)ERROR_INVALID_COMMAND:
            return "Invalid command format";
        case (int)ERROR_SENSOR_NOT_FOUND:
            return "Sensor not found or not registered";
        case (int)ERROR_MEASUREMENT_NOT_SUPPORTED:
            return "Measurement type not supported by sensor";
        case (int)ERROR_SENSOR_INIT_FAILED:
            return "Sensor initialization failed";
        case (int)ERROR_SENSOR_READ_FAILED:
            return "Sensor reading failed";
        case (int)ERROR_NULL_POINTER:
            return "Null pointer error";
        case (int)ERROR_EMPTY_PARAMETER:
            return "Empty or invalid parameter";
        case (int)ERROR_COMMUNICATION_FAILED:
            return "Communication with sensor failed";
        case (int)ERROR_UNKNOWN:
        default:
            return "Unknown error";
    }
}

void logError(float errorCode, const String& context) {
    String message = "ERROR [" + String((int)errorCode) + "]: " + getErrorMessage(errorCode);
    if (context.length() > 0) {
        message += " (Context: " + context + ")";
    }
    Serial.println(message);
    
    // Provide recovery suggestions for common errors
    String recovery = getRecoveryAction(errorCode);
    if (recovery.length() > 0) {
        Serial.println("  Suggestion: " + recovery);
    }
}

void logWarning(const String& message, const String& context) {
    String fullMessage = "WARNING: " + message;
    if (context.length() > 0) {
        fullMessage += " (Context: " + context + ")";
    }
    Serial.println(fullMessage);
}

bool isRecoverableError(float errorCode) {
    switch ((int)errorCode) {
        case (int)ERROR_SENSOR_READ_FAILED:
        case (int)ERROR_COMMUNICATION_FAILED:
        case (int)ERROR_SENSOR_INIT_FAILED:
            return true;  // These errors might be temporary
        case (int)ERROR_INVALID_COMMAND:
        case (int)ERROR_SENSOR_NOT_FOUND:
        case (int)ERROR_MEASUREMENT_NOT_SUPPORTED:
        case (int)ERROR_NULL_POINTER:
        case (int)ERROR_EMPTY_PARAMETER:
            return false; // These are permanent errors
        default:
            return false;
    }
}

String getRecoveryAction(float errorCode) {
    switch ((int)errorCode) {
        case (int)ERROR_SENSOR_INIT_FAILED:
            return "Check sensor wiring, power supply, and I2C connections";
        case (int)ERROR_SENSOR_READ_FAILED:
            return "Retry the operation or check sensor status";
        case (int)ERROR_COMMUNICATION_FAILED:
            return "Check I2C bus and sensor connections";
        case (int)ERROR_SENSOR_NOT_FOUND:
            return "Verify sensor is registered and spelled correctly";
        case (int)ERROR_MEASUREMENT_NOT_SUPPORTED:
            return "Check supported measurements for this sensor type";
        case (int)ERROR_INVALID_COMMAND:
            return "Use format: sensor:type:measurement (e.g., sensor:hdc1080:temperature)";
        default:
            return "";
    }
}