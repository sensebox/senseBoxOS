#include "commands.h"
#include "peripherals/display.h"
#include "peripherals/led.h"
#include "logic/time.h"

typedef void (*CommandHandler)(String args);
std::map<String, CommandHandler> commandMap;

void setupCommandMap() {
  commandMap["display"] = handleDisplay;
  commandMap["led"]     = handleLed;
  commandMap["delay"]   = handleDelay;
}