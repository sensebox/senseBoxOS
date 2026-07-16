#pragma once

#include <Arduino.h>
#include <map>

// Untyped variable that can hold either a number or string
struct Variable {
    enum Type { TYPE_NONE, TYPE_NUMBER, TYPE_STRING } type;
    float numberValue;
    String stringValue;
    
    Variable() : type(TYPE_NONE), numberValue(0.0), stringValue("") {}
    Variable(float n) : type(TYPE_NUMBER), numberValue(n), stringValue("") {}
    Variable(const String& s) : type(TYPE_STRING), numberValue(0.0), stringValue(s) {}
};

extern std::map<String, Variable> variables;

void setVar(String name, float value);
void setVar(String name, const String& value);
float getVar(String name);
String getVarString(String name);
bool isVarString(String name);
