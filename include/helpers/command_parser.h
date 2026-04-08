#pragma once

#include <Arduino.h>
#include "helpers/error_codes.h"

/**
 * Structure to hold parsed sensor command components
 */
struct SensorCommand {
    String sensorType;    // "hdc1080", "bme680"
    String measurement;   // "temperature", "humidity", "pressure"
    bool isValid;         // true if command was parsed successfully
    float errorCode;      // error code if parsing failed
    String errorMessage;  // detailed error message
    
    SensorCommand() : isValid(false), errorCode(ERROR_INVALID_COMMAND) {}
    SensorCommand(const String& type, const String& measure) 
        : sensorType(type), measurement(measure), isValid(true), errorCode(ERROR_SUCCESS) {}
    SensorCommand(float error, const String& message) 
        : isValid(false), errorCode(error), errorMessage(message) {}
};

/**
 * Parse a sensor command string into its components
 * Expected format: "sensor:hdc1080:temperature"
 * @param command The command string to parse
 * @return SensorCommand struct with parsed components
 */
SensorCommand parseSensorCommand(const String& command);

/**
 * Check if a command string is a sensor command
 * @param command The command string to check
 * @return true if command starts with "sensor:", false otherwise
 */
bool isSensorCommand(const String& command);

/**
 * Validate sensor command format
 * @param command The command string to validate
 * @return true if format is valid (has exactly 3 parts separated by colons)
 */
bool isValidSensorCommandFormat(const String& command);