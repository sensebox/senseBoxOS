#ifndef CONDITIONAL_H
#define CONDITIONAL_H

#include <Arduino.h>

void setVar(String name, float value);
float getVar(String name);
float evalNumber(String expr);
bool  evalCond(String cond);

#endif 
