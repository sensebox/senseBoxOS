#pragma once

#include <map>
#include <Arduino.h>

typedef void (*CommandHandler)(String args);
extern std::map<String, CommandHandler> commandMap;
void setupCommandMap();
