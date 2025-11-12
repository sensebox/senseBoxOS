#include "communication/serial.h"

void SerialModule::setup() {
    Serial.begin(115200);
}

bool SerialModule::begin() {
    // while(!Serial) ;
    Serial.println("senseBoxOS ready");
    return true;
}

void SerialModule::loop() {
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
