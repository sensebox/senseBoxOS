#include <Arduino.h>

#include <SPI.h>
#include <Wire.h>

#include "commands.h"
#include "communication/ble.h"
#include "communication/serial.h"

#include "helpers/block.h"
#include "helpers/interpreter.h"
#include "helpers/sensor_registry.h"

#include "logic/eval.h"
#include "logic/var.h"
#include "logic/time.h"
#include "peripherals/display.h"
#include "peripherals/led.h"
#include "peripherals/button.h"
#include "peripherals/sensors/hdc.h"
#include "peripherals/sensors/bme680.h"

void setup() {
  serialModule.setup();

  setupCommandMap();
  initLedRGB();
  initDisplay();
  initButton();
  Wire.begin();

  // Initialize BLE first (needed for device ID)
  bleModule.setup();
  
  // Get and cache device ID, then display it
  String cachedId = getDeviceID();
  displayDeviceID();
  delay(2000);  // Show ID for 2 seconds

  // blink LED to show that senseBoxOS is running
  delay(100);
  setLedRGB(250, 0, 250);
  delay(100);
  setLedRGB(0, 0, 0);
  delay(100);
  setLedRGB(250, 0, 250);
  delay(100);
  setLedRGB(0, 0, 0);

  bleModule.begin(cachedId);  // Pass device ID to BLE
  serialModule.begin();
}

void loop() {
  bleModule.loop();
  serialModule.loop();

}
