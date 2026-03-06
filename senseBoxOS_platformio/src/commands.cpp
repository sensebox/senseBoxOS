#include "commands.h"
#include "peripherals/display.h"
#include "peripherals/led.h"
#include "logic/time.h"
#include "helpers/command_parser.h"

typedef void (*CommandHandler)(String args);
std::map<String, CommandHandler> commandMap;

// Example sensor command handler using the command parser
void handleSensorCommand(String args) {
  SensorCommand cmd = parseSensorCommand(args);
  
  if (!cmd.isValid) {
    Serial.println("Invalid sensor command format. Use: sensor:type:measurement");
    return;
  }
  
  Serial.print("Sensor command parsed - Type: ");
  Serial.print(cmd.sensorType);
  Serial.print(", Measurement: ");
  Serial.println(cmd.measurement);
  
  // Here you would implement the actual sensor reading logic
  // For now, just acknowledge the command
}

void setupCommandMap() {
  commandMap["display"] = handleDisplay;
  commandMap["clearDisplay"] = handleClearDisplay;
  commandMap["displayMeasurement"] = handleDisplayMeasurement;
  commandMap["led"]     = handleLed;
  commandMap["randomLed"] = handleRandomLed;
  commandMap["delay"]   = handleDelay;
  commandMap["sensor"]  = handleSensorCommand;
}