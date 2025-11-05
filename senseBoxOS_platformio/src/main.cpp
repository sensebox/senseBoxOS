#include <Arduino.h>

#include <SPI.h>
#include <Wire.h>

#include "commands.h"

#include "helpers/block.h"
#include "helpers/interpreter.h"
#include "helpers/sensor_registry.h"

#include "logic/eval.h"
#include "logic/var.h"
#include "logic/time.h"
#include "peripherals/display.h"
#include "peripherals/led.h"
#include "peripherals/sensors/hdc.h"
#include "peripherals/sensors/bme680.h"

// Global sensor registry instance
SensorRegistry sensorRegistry;

void setup() {
  Serial.begin(115200);

  setupCommandMap();
  initLedRGB();
  initDisplay();
  Wire.begin();

  // blink LED to show that senseBoxOS is running
  delay(100);
  setLedRGB(250, 0, 250);
  delay(100);
  setLedRGB(0, 0, 0);
  delay(100);
  setLedRGB(250, 0, 250);
  delay(100);
  setLedRGB(0, 0, 0);

  while (!Serial);
  
  Serial.println("senseBoxOS ready");
}

void loop() {
  if (Serial.available()) {
    static String line;
    char c = Serial.read();
    if (c == '\n') {
      line.trim();
      if (line == "RUN") {
        runningScript = true; runForever = false; runScript();
        runningScript = false;
      } else if (line == "RUNLOOP") {
        runningScript = true; runForever = true;  runScript();
      } else if (line == "STOP") {
        runForever = false; runningScript = false;
        scriptLines.clear();
        variables.clear();
        Serial.println("Stopped");
      } else if (line.length() > 0) {
        scriptLines.push_back(line);
      }
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }
}
