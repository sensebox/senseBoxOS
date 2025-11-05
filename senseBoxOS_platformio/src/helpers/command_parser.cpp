#include "helpers/command_parser.h"

SensorCommand parseSensorCommand(const String& command) {
    // Check for empty command
    if (command.length() == 0) {
        String error = "Command is empty";
        logError(ERROR_EMPTY_PARAMETER, "parseSensorCommand: " + error);
        return SensorCommand(ERROR_EMPTY_PARAMETER, error);
    }
    
    // Check if it's a sensor command first
    if (!isSensorCommand(command)) {
        String error = "Command does not start with 'sensor:' - got: '" + command + "'";
        logError(ERROR_INVALID_COMMAND, "parseSensorCommand: " + error);
        return SensorCommand(ERROR_INVALID_COMMAND, error);
    }
    
    // Validate format
    if (!isValidSensorCommandFormat(command)) {
        String error = "Invalid command format. Expected 'sensor:type:measurement' - got: '" + command + "'";
        logError(ERROR_INVALID_COMMAND, "parseSensorCommand: " + error);
        return SensorCommand(ERROR_INVALID_COMMAND, error);
    }
    
    // Split the command by colons
    int firstColon = command.indexOf(':');
    int secondColon = command.indexOf(':', firstColon + 1);
    
    // Extract sensor type and measurement
    String sensorType = command.substring(firstColon + 1, secondColon);
    String measurement = command.substring(secondColon + 1);
    
    // Trim whitespace
    sensorType.trim();
    measurement.trim();
    
    // Validate that we have non-empty components
    if (sensorType.length() == 0) {
        String error = "Sensor type is empty in command: '" + command + "'";
        logError(ERROR_EMPTY_PARAMETER, "parseSensorCommand: " + error);
        return SensorCommand(ERROR_EMPTY_PARAMETER, error);
    }
    
    if (measurement.length() == 0) {
        String error = "Measurement type is empty in command: '" + command + "'";
        logError(ERROR_EMPTY_PARAMETER, "parseSensorCommand: " + error);
        return SensorCommand(ERROR_EMPTY_PARAMETER, error);
    }
    
    Serial.println("Successfully parsed command: sensor=" + sensorType + ", measurement=" + measurement);
    return SensorCommand(sensorType, measurement);
}

bool isSensorCommand(const String& command) {
    return command.startsWith("sensor:");
}

bool isValidSensorCommandFormat(const String& command) {
    // Count colons - should have exactly 2
    int colonCount = 0;
    for (int i = 0; i < command.length(); i++) {
        if (command.charAt(i) == ':') {
            colonCount++;
        }
    }
    
    // Must have exactly 2 colons for "sensor:type:measurement" format
    if (colonCount != 2) {
        return false;
    }
    
    // Check that it starts with "sensor:"
    if (!command.startsWith("sensor:")) {
        return false;
    }
    
    // Find colon positions
    int firstColon = command.indexOf(':');
    int secondColon = command.indexOf(':', firstColon + 1);
    
    // Validate positions and ensure we have content between colons
    if (firstColon == -1 || secondColon == -1 || 
        secondColon <= firstColon + 1 || 
        secondColon >= command.length() - 1) {
        return false;
    }
    
    return true;
}