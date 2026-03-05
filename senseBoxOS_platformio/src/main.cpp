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

BME680Sensor BME680Sensor;

void setup() {
  serialModule.setup();

  setupCommandMap();
  initLedRGB();
  initDisplay();
  BME680Sensor.begin();
  
  // Register the global BME680 sensor instance with the registry
  sensorRegistry.registerSensor("bme680", &BME680Sensor);

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
  BME680Sensor.updateSensorData();
  serialModule.loop();

}
