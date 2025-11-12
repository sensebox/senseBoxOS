#include "communication/protocol.h"

// This file provides the base implementation for communication protocols
// Individual implementations will inherit from this interface
// and provide their specific protocol logic

// The BaseProtocol class provides common functionality:
// - communication setup
// - commmunication begin
// - communication loop processing