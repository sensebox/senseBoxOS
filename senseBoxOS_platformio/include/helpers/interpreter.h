#pragma once

#include <Arduino.h>
#include <vector>
#include <map>
#include "helpers/sensor_registry.h"

extern std::map<String, float> variables;
extern std::vector<String> scriptLines;
extern bool runningScript;
extern bool runForever;   // LOOP mode
extern unsigned int lineDelay;  // Delay in ms between lines
extern SensorRegistry sensorRegistry;

void executeLine(String line, int& pc);
void ignoreLine(String line, int& pc);
void runScript();
void pumpControl();
