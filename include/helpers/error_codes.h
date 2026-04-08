#pragma once

#include <Arduino.h>

/**
 * Standardized error codes for the modular command system
 * All error values are negative to distinguish from valid sensor readings
 */

// General error codes
#define ERROR_SUCCESS 0.0f
#define ERROR_UNKNOWN -999.0f
#define ERROR_INVALID_COMMAND -998.0f
#define ERROR_SENSOR_NOT_FOUND -997.0f
#define ERROR_MEASUREMENT_NOT_SUPPORTED -996.0f
#define ERROR_SENSOR_INIT_FAILED -995.0f
#define ERROR_SENSOR_READ_FAILED -994.0f
#define ERROR_NULL_POINTER -993.0f
#define ERROR_EMPTY_PARAMETER -992.0f
#define ERROR_COMMUNICATION_FAILED -991.0f

/**
 * Check if a value represents an error
 * @param value The value to check
 * @return true if the value is an error code (negative)
 */
inline bool isError(float value) {
    return value < 0.0f;
}

/**
 * Get a human-readable error message for an error code
 * @param errorCode The error code
 * @return String describing the error
 */
String getErrorMessage(float errorCode);

/**
 * Log an error with context information
 * @param errorCode The error code
 * @param context Additional context (e.g., sensor type, command)
 */
void logError(float errorCode, const String& context = "");

/**
 * Log a warning message
 * @param message The warning message
 * @param context Additional context information
 */
void logWarning(const String& message, const String& context = "");

/**
 * Check if an error is recoverable (can be retried)
 * @param errorCode The error code to check
 * @return true if the error might be recoverable with retry
 */
bool isRecoverableError(float errorCode);

/**
 * Get suggested recovery action for an error
 * @param errorCode The error code
 * @return String with suggested recovery action
 */
String getRecoveryAction(float errorCode);