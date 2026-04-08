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
