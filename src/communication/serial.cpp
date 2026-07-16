#include "communication/serial.h"
#include "communication/protocol.h"

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
    while (Serial.available()) {
        char c = Serial.read();
        commandBuffer.processChar(c);
    }
    
    // Check for idle flush
    commandBuffer.checkIdleFlush();
}

void SerialModule::sendValue(const String& label, float value) {
    // Distinctive prefix so the host can filter data lines from debug output
    Serial.printf("DATA:%s=%.2f\n", label.c_str(), value);
}
