#include "logic/eval.h"
#include "helpers/interpreter.h"
#include "helpers/command_parser.h"
#include "helpers/sensor_registry.h"

// Global sensor registry instance (declared in main.cpp)
extern SensorRegistry sensorRegistry;

// Simple arithmetic evaluator: + - * / (left-to-right)
float evalNumber(String expr) {
  expr.trim();
  
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