#include <Arduino.h>

#include <SPI.h>
#include <Wire.h>

#include "commands.h"

#include "helpers/block.h"
#include "helpers/interpreter.h"

#include "logic/eval_var.h"
#include "logic/time.h"
#include "peripherals/hdc.h"
#include "peripherals/display.h"
#include "peripherals/led.h"

// ===== Arduino setup/loop =====
void setup() {
  Serial.begin(115200);
  while (!Serial);

  setupCommandMap();

  initLedRGB();

  initDisplay();

  Serial.println("Ready. Send lines, then RUN or RUNLOOP. Use STOP to halt.");
}

void loop() {
  if (Serial.available()) {
    static String line;
    char c = Serial.read();
    if (c == '\n') {
      line.trim();
      Serial.print("Got line: "); Serial.println(line);
      if (line == "RUN") {
        runningScript = true; runForever = false; runScript();
        runningScript = false;
      } else if (line == "RUNLOOP") {
        runningScript = true; runForever = true;  runScript();
      } else if (line == "STOP") {
        runForever = false; runningScript = false;
        scriptLines.clear();
        variables.clear();
        Serial.println("Stopped and cleared script & variables.");
      } else if (line.length() > 0) {
        scriptLines.push_back(line);
      }
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }
}
