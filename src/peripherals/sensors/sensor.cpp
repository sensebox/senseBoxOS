#include "peripherals/sensors/sensor.h"

// This file provides the base implementation for Sensor
// Individual sensor implementations will inherit from this interface
// and provide their specific measurement reading logic

// The BaseSensor class provides common functionality:
// - Initialization tracking
// - Measurement type validation
// - Error handling and logging
// - Standardized error return values (-999.0f)