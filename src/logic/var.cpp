#include "logic/var.h"

// Global untyped variables storage
std::map<String, Variable> variables;

void setVar(String name, float value) { 
  variables[name] = Variable(value);
  Serial.printf("[VAR] SET %s = %.2f (number)\n", name.c_str(), value);
}

void setVar(String name, const String& value) {
  variables[name] = Variable(value);
  Serial.printf("[VAR] SET %s = \"%s\" (string)\n", name.c_str(), value.c_str());
}

float getVar(String name) { 
  if (!variables.count(name)) {
    Serial.printf("[VAR] GET %s = 0.0 (not found, returning number)\n", name.c_str());
    return 0.0;
  }
  
  Variable& var = variables[name];
  if (var.type == Variable::TYPE_NUMBER) {
    Serial.printf("[VAR] GET %s = %.2f (number, found)\n", name.c_str(), var.numberValue);
    return var.numberValue;
  } else {
    // String stored but number requested - convert
    float result = var.stringValue.toFloat();
    Serial.printf("[VAR] GET %s = %.2f (string \"%s\" converted to number)\n", name.c_str(), result, var.stringValue.c_str());
    return result;
  }
}

String getVarString(String name) {
  if (!variables.count(name)) {
    Serial.printf("[VAR] GET_STRING %s = \"\" (not found)\n", name.c_str());
    return "";
  }
  
  Variable& var = variables[name];
  if (var.type == Variable::TYPE_STRING) {
    Serial.printf("[VAR] GET_STRING %s = \"%s\" (string, found)\n", name.c_str(), var.stringValue.c_str());
    return var.stringValue;
  } else {
    // Number stored but string requested - convert
    String result = String(var.numberValue);
    Serial.printf("[VAR] GET_STRING %s = \"%s\" (number %.2f converted to string)\n", name.c_str(), result.c_str(), var.numberValue);
    return result;
  }
}

bool isVarString(String name) {
  if (!variables.count(name)) return false;
  return variables[name].type == Variable::TYPE_STRING;
}
