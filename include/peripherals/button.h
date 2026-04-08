#pragma once

#include <Arduino.h>
#include <JC_Button.h>

// Initialize button system
void initButton();

// Check if button on specified pin is pressed
bool isButtonPressed(int pin);
