#include "peripherals/button.h"
#include <map>

// Map to store Button objects by pin number
std::map<int, Button*> buttons;

void initButton() {
  // Initial setup if needed
  // Buttons will be created on-demand when first accessed
}

bool isButtonPressed(int pin) {
  // Create button instance if it doesn't exist yet
  if (buttons.find(pin) == buttons.end()) {
    buttons[pin] = new Button(pin);
    buttons[pin]->begin();
    Serial.printf("[BUTTON] Created button on pin %d\n", pin);
  }
  
  // Read current button state
  buttons[pin]->read();
  
  // Return whether button is currently pressed
  bool pressed = buttons[pin]->isPressed();
  Serial.printf("[BUTTON] Pin %d state: %s\n", pin, pressed ? "PRESSED" : "released");
  return pressed;
}
