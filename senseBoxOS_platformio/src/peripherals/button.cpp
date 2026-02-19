#include "peripherals/button.h"
#include <map>

// Map to store Button objects by pin number
std::map<int, Button*> buttons;
// Track last state to only log on changes
std::map<int, bool> lastButtonState;

void initButton() {
  // Initial setup if needed
  // Buttons will be created on-demand when first accessed
}

bool isButtonPressed(int pin) {
  // Create button instance if it doesn't exist yet
  if (buttons.find(pin) == buttons.end()) {
    buttons[pin] = new Button(pin);
    buttons[pin]->begin();
    lastButtonState[pin] = false;
    Serial.printf("[BUTTON] Created button on pin %d\n", pin);
  }
  
  // Read current button state
  buttons[pin]->read();
  
  // Return whether button is currently pressed
  bool pressed = buttons[pin]->isPressed();
  
  // Only log when state changes
  if (lastButtonState[pin] != pressed) {
    lastButtonState[pin] = pressed;
  }
  
  return pressed;
}
