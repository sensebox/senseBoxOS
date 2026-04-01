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
  
  // Try to initialize BME680, only register if available
  if (BME680Sensor.begin()) {
    sensorRegistry.registerSensor("bme680", &BME680Sensor);
  } else {
    Serial.println("BME680 not available - sensor not registered");
  }

  initButton();
  Wire.begin();

  // Show startup message immediately (assume no BLE for fastest feedback)
  Serial.println("Showing initial display message...");
  displaySerialOnlyMode();

  // Initialize BLE (to detect if BLE module is present)
  bleModule.setup();
  
  // Update display if BLE is available
  if (bleModule.isAvailable()) {
    String cachedId = getDeviceID();
    displayDeviceID();
    delay(2000);  // Show ID for 2 seconds
  } else {
    // BLE not available - keep the Serial-Only message
    Serial.println("BLE not available - keeping Serial-Only mode message");
    delay(2000);  // Show message for 2 seconds
  }

  // blink LED to show that senseBoxOS is running
  delay(100);
  setLedRGB(250, 0, 250);
  delay(100);
  setLedRGB(0, 0, 0);
  delay(100);
  setLedRGB(250, 0, 250);
  delay(100);
  setLedRGB(0, 0, 0);

  // Only begin BLE advertising if BLE is available
  if (bleModule.isAvailable()) {
    String cachedId = getDeviceID();
    bleModule.begin(cachedId);  // Pass device ID to BLE
  }
  
  serialModule.begin();
}

void loop() {
  bleModule.loop();
  
  // Only update BME680 if sensor is available
  if (BME680Sensor.isAvailable()) {
    BME680Sensor.updateSensorData();
  }
  
  serialModule.loop();
}
