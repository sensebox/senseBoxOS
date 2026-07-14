#pragma once

#include <Arduino.h>
#include <vector>
#include <map>
#include "helpers/sensor_registry.h"
#include "helpers/script_parser.h"
#include "logic/var.h"  // For Variable type

extern std::vector<String> scriptLines;  // Legacy: all lines combined
extern std::vector<String> setupLines;   // New: setup block only
extern std::vector<String> loopLines;    // New: loop block only
extern bool hasSetup;                    // Flag if setup block exists
extern bool hasLoop;                     // Flag if loop block exists
extern bool setupExecuted;               // Flag if setup has been run
extern bool runningScript;
extern bool runForever;   // LOOP mode
extern unsigned int lineDelay;  // Delay in ms between lines
extern SensorRegistry sensorRegistry;

void executeLine(String line, int& pc);
void ignoreLine(String line, int& pc);
void runScript();
void runSetupBlock();
void runLoopBlock();
void pumpControl();
