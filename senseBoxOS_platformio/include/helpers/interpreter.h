#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <Arduino.h>
#include <vector>
#include <map>

extern std::map<String, float> variables;
extern std::vector<String> scriptLines;
extern bool runningScript;
extern bool runForever;   // RUNLOOP mode

void executeLine(String line, int& pc);
void runScript();
bool pumpControl();

#endif