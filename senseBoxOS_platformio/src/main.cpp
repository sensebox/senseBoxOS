#include <Arduino.h>

#include <SPI.h>
#include <Wire.h>
#include <SenseBoxBLE.h>

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

// ===== BLE Configuration =====
static const char* BLE_SERVICE_UUID = "CF06A218F68EE0BEAD048EBC1EB0BC84";
static const char* BLE_RX_UUID      = "2CDF217435BEFDC44CA26FD173F8B3A8";

// BLE line assembly state
String bleBuffer;
uint32_t lastBleByteMs = 0;
const uint32_t BLE_IDLE_FLUSH_MS = 800;
int bleParenDepth = 0;
bool bleSawOpenParen = false;

// USB line buffer
String usbBuffer;

// UTF-16LE decoder (ASCII subset for commands)
String utf16leToString(uint8_t* data, size_t length) {
  String result = "";
  if (length % 2 != 0) length--;
  
  for (size_t i = 0; i < length; i += 2) {
    uint16_t utf16_char = data[i] | (data[i + 1] << 8);
    if (utf16_char == 0x0000) break;
    if (utf16_char <= 0x007F) {
      result += (char)utf16_char;
    }
  }
  return result;
}

// Process a complete line from any source
void processLine(const String& line, const char* tag) {
  String s = line;
  s.trim();
  if (s.length() == 0) return;
  
  if (runningScript) {
    if (s == "STOP") {
      runForever = false;
      runningScript = false;
      scriptLines.clear();
      variables.clear();
      Serial.printf("[%s] STOP received. Cleared script & variables.\n", tag);
    } else {
      Serial.printf("[%s] (ignored while running): %s\n", tag, s.c_str());
    }
    return;
  }
  
  if (s == "DISCONNECT") {
    Serial.println("Disconnect command received. Restarting...");
    delay(100);
    ESP.restart();
  } else if (s == "RUN") {
    runningScript = true;
    runForever = false;
    runScript();
    runningScript = false;
  } else if (s == "RUNLOOP") {
    runningScript = true;
    runForever = true;
    runScript();
  } else if (s == "STOP") {
    runForever = false;
    runningScript = false;
    scriptLines.clear();
    variables.clear();
    Serial.printf("[%s] STOP (idle). Cleared.\n", tag);
  } else {
    scriptLines.push_back(s);
    Serial.printf("[%s] Queued: %s\n", tag, s.c_str());
  }
}

// Flush BLE buffer to processing
void bleFlush(const char* reason) {
  String s = bleBuffer;
  s.trim();
  if (s.length() == 0) return;
  
  Serial.printf("[BLE] FLUSH (%s): \"%s\"\n", reason, s.c_str());
  processLine(s, "BLE");
  
  bleBuffer = "";
  bleParenDepth = 0;
  bleSawOpenParen = false;
}

// Check for idle timeout and flush if needed
void bleMaybeFlushByIdle() {
  if (bleBuffer.length() == 0) return;
  if (bleParenDepth > 0) return;
  
  uint32_t now = millis();
  if (now - lastBleByteMs >= BLE_IDLE_FLUSH_MS) {
    bleFlush("idle");
  }
}

// BLE write callback
void onBleConfigWrite() {
  uint8_t raw[64] = {0};
  SenseBoxBLE::read(raw, sizeof(raw));
  
  String chunk = utf16leToString(raw, sizeof(raw));
  Serial.printf("[BLE] chunk: \"%s\"\n", chunk.c_str());
  
  for (int i = 0; i < chunk.length(); ++i) {
    char c = chunk[i];
    
    // Ignore CR/LF/NUL (some apps send after every keystroke)
    if (c == '\r' || c == '\n' || c == '\0') continue;
    
    bleBuffer += c;
    
    // Track parentheses for function calls like led(255,0,0)
    if (c == '(') {
      bleParenDepth++;
      bleSawOpenParen = true;
    } else if (c == ')') {
      if (bleParenDepth > 0) bleParenDepth--;
    }
    
    // Immediate flush when function call completes
    if (c == ')' && bleParenDepth == 0 && bleSawOpenParen) {
      bleFlush("paren");
    }
  }
  
  lastBleByteMs = millis();
}

// Poll BLE and check for idle flush
void pollBle() {
  SenseBoxBLE::poll();
  bleMaybeFlushByIdle();
}

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
  
  // Initialize BLE
  SenseBoxBLE::start("senseBox-BLE");
  int rxHandle = SenseBoxBLE::setConfigCharacteristic(BLE_SERVICE_UUID, BLE_RX_UUID);
  if (rxHandle <= 0) {
    Serial.println("[BLE] Warning: setConfigCharacteristic failed or already set");
  }
  SenseBoxBLE::configHandler = onBleConfigWrite;
  SenseBoxBLE::setName("senseBox-BLE");
  SenseBoxBLE::advertise();
  
  Serial.println("senseBoxOS ready (Serial + BLE)");
  Serial.printf("BLE Service: %s\n", BLE_SERVICE_UUID);
  Serial.printf("BLE RX Char: %s\n", BLE_RX_UUID);
}

void loop() {
  // Handle USB Serial input
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      usbBuffer.trim();
      if (usbBuffer.length() > 0) {
        Serial.printf("[USB] FULL: \"%s\"\n", usbBuffer.c_str());
        processLine(usbBuffer, "USB");
      }
      usbBuffer = "";
    } else if (c != '\r') {
      usbBuffer += c;
    }
  }
  
  // Poll BLE for incoming data
  if (!runningScript) {
    pollBle();
  }
}
