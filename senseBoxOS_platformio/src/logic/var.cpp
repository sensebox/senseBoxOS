#include "logic/var.h"
#include "helpers/interpreter.h"

void setVar(String name, float value) { 
  variables[name] = value; 
}

float getVar(String name) { 
  return variables.count(name) ? variables[name] : 0; 
}
