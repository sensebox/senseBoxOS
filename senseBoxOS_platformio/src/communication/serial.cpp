#include "communication/serial.h"

SerialModule serialModule;

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
                line = "";
                if (runningScript) {
                    Serial.println("[Serial] Script already running, stopping current script first...");
                    runningScript = false;
                    runForever = false;
                    delay(100); // Brief delay to allow current iteration to finish
                    scriptLines.clear();
                    variables.clear();
                }
                Serial.println("========== EXECUTING SCRIPT ==========");
                Serial.printf("Script has %d lines:\n", scriptLines.size());
                for (int i = 0; i < scriptLines.size(); i++) {
                    Serial.printf("  [%d]: %s\n", i, scriptLines[i].c_str());
                }
                Serial.println("======================================");
                runningScript = true; 
                runForever = false; 
                runScript();
                runningScript = false;
            } else if (line == "LOOP") {
                line = "";
                if (runningScript) {
                    Serial.println("[Serial] Script already running, stopping current script first...");
                    runningScript = false;
                    runForever = false;
                    delay(100); // Brief delay to allow current iteration to finish
                    scriptLines.clear();
                    variables.clear();
                }
                Serial.println("========== EXECUTING SCRIPT (LOOP) ==========");
                Serial.printf("Script has %d lines:\n", scriptLines.size());
                for (int i = 0; i < scriptLines.size(); i++) {
                    Serial.printf("  [%d]: %s\n", i, scriptLines[i].c_str());
                }
                Serial.println("=============================================");
                runningScript = true; 
                runForever = true;
                runScript();
            } else if (line == "STOP") {
                line = "";
                runForever = false;
                runningScript = false;
                scriptLines.clear();
                variables.clear();
                Serial.println("Stopped");
            } else if (line.length() > 0) {
                // Ignore new script lines while a script is running
                if (runningScript) {
                    Serial.printf("[Serial] Ignoring line while script is running: \"%s\"\n", line.c_str());
                    line = "";
                } else {
                    scriptLines.push_back(line);
                    line = "";
                }
            }
        } else if (c != '\r') {
            line += c;
        }
    }
}
