#include "logic/eval.h"
#include "helpers/interpreter.h"
#include "helpers/command_parser.h"
#include "helpers/sensor_registry.h"
#include "peripherals/button.h"
#include "peripherals/sensors/accelerometer.h"

// Global sensor registry instance (declared in main.cpp)
extern SensorRegistry sensorRegistry;

// Global accelerometer instance for shake detection
AccelerometerSensor* globalAccelerometer = nullptr;

bool isBoxShaken() {
  if (globalAccelerometer == nullptr) {
    globalAccelerometer = new AccelerometerSensor();
  }
  return globalAccelerometer->isShaken();
}

// Simple arithmetic evaluator: + - * / (left-to-right)
float evalNumber(String expr) {
  expr.trim();
  
  // Check for boxShaken() function
    if (expr.startsWith("boxShaken(") && expr.endsWith(")")) {
    bool shaken = isBoxShaken();
    return shaken ? 1.0 : 0.0;
  }
  
  // Check for buttonPressed(pin) function
  if (expr.startsWith("buttonPressed(") && expr.endsWith(")")) {
    int startPos = expr.indexOf('(') + 1;
    int endPos = expr.indexOf(')');
    String pinStr = expr.substring(startPos, endPos);
    pinStr.trim();
    
    // Default to pin 0 (Boot button on ESP32-S2) if no pin specified
    int pin = 0;
    if (pinStr.length() > 0) {
      pin = pinStr.toInt();
    }
    
    bool pressed = isButtonPressed(pin);
    return pressed ? 1.0 : 0.0;
  }
  
  // First check for arithmetic operations
  int opPos = -1; char op = 0;
  for (int i = 1; i < expr.length() - 1; ++i) {
    char c = expr[i];
    if (c=='+'||c=='-'||c=='*'||c=='/') { op=c; opPos=i; break; }
  }
  if (opPos != -1) {
    String left  = expr.substring(0, opPos);  left.trim();
    String right = expr.substring(opPos + 1); right.trim();
    float l = evalNumber(left);
    float r = evalNumber(right);
    switch (op) { case '+': return l+r; case '-': return l-r; case '*': return l*r; case '/': return (r!=0)? l/r : 0; }
  }
  
  // Check if it's a sensor command
  if (isSensorCommand(expr)) {
    SensorCommand cmd = parseSensorCommand(expr);
    if (cmd.isValid) {
      return sensorRegistry.readSensor(cmd.sensorType, cmd.measurement);
    } else {
      return cmd.errorCode;
    }
  }
  
  if (variables.count(expr)) return variables[expr];
  return expr.toFloat();
}

// Boolean condition evaluator: >, <, >=, <=, ==, !=
bool evalCond(String cond) {
  cond.trim();
  const char* ops[] = {">=","<=","==","!=",">","<"};
  for (int i = 0; i < 6; i++) {
    String op = ops[i];
    int pos = cond.indexOf(op);
    if (pos != -1) {
      String L = cond.substring(0, pos);  L.trim();
      String R = cond.substring(pos + op.length()); R.trim();
      float l = evalNumber(L), r = evalNumber(R);
      if      (op == ">=") return l >= r;
      else if (op == "<=") return l <= r;
      else if (op == "==") return l == r;
      else if (op == "!=") return l != r;
      else if (op == ">")  return l >  r;
      else if (op == "<")  return l <  r;
    }
  }
  return evalNumber(cond) != 0;
}