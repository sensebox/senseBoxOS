#include "commands.h"
#include "peripherals/display.h"
#include "peripherals/led.h"
#include "logic/time.h"
#include "logic/eval.h"
#include "helpers/command_parser.h"
#include "communication/ble.h"
#include "communication/serial.h"

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

// Find the top-level argument separator (comma at parenthesis depth 0).
// This keeps commas inside nested calls like random(1,5) intact.
static int findArgSeparator(const String& args) {
  int depth = 0;
  for (int i = 0; i < (int)args.length(); i++) {
    char c = args[i];
    if (c == '(') depth++;
    else if (c == ')') { if (depth > 0) depth--; }
    else if (c == ',' && depth == 0) return i;
  }
  return -1;
}

// Send a measured value back to the host over BLE.
// Example: sendBLE(sensor:bme680:temperature, 1)
// The optional identifier lets the host map the value to a channel.
// Sent as an 8-byte packet: [identifier(float)][value(float)].
void handleSendBLE(String args) {
  args.trim();
  int sep = findArgSeparator(args);
  if (sep == -1) {
    // No identifier provided: send the value only (backwards compatible).
    bleModule.sendValue(evalNumber(args));
    return;
  }
  String expr = args.substring(0, sep); expr.trim();
  String idStr = args.substring(sep + 1); idStr.trim();
  float value = evalNumber(expr);
  float identifier = evalNumber(idStr);
  bleModule.sendValue(identifier, value);
}

// Send a measured value back to the host over Serial.
// Example: sendSerial(sensor:bme680:temperature, temp)
// The optional identifier is used as the label: DATA:<identifier>=<value>.
void handleSendSerial(String args) {
  args.trim();
  int sep = findArgSeparator(args);
  if (sep == -1) {
    // No identifier provided: use the expression itself as the label.
    serialModule.sendValue(args, evalNumber(args));
    return;
  }
  String expr = args.substring(0, sep); expr.trim();
  String idStr = args.substring(sep + 1); idStr.trim();
  float value = evalNumber(expr);
  serialModule.sendValue(idStr, value);
}

void setupCommandMap() {
  commandMap["display"] = handleDisplay;
  commandMap["clearDisplay"] = handleClearDisplay;
  commandMap["displayMeasurement"] = handleDisplayMeasurement;
  commandMap["drawPixel"] = handleDrawPixel;
  commandMap["drawBitmap"] = handleDrawBitmap;
  commandMap["displayMatrix"] = handleDisplayMatrix;
  commandMap["led"]     = handleLed;
  commandMap["randomLed"] = handleRandomLed;
  commandMap["delay"]   = handleDelay;
  commandMap["setLineDelay"] = handleSetLineDelay;
  commandMap["sensor"]  = handleSensorCommand;
  commandMap["sendBLE"] = handleSendBLE;
  commandMap["sendSerial"] = handleSendSerial;
}