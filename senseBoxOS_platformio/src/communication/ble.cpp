#include "communication/ble.h"

BLEModule bleModule;

// ===== BLE UUIDs =====
static const char* BLE_SERVICE_UUID = "CF06A218F68EE0BEAD048EBC1EB0BC84";
static const char* BLE_RX_UUID      = "2CDF217435BEFDC44CA26FD173F8B3A8";
String bleBuf; 
int  bleParenDepth = 0;
bool bleSawOpenParen = false;
uint32_t lastBleByteMs = 0;
const uint32_t BLE_IDLE_FLUSH_MS = 800;  // idle gap before flushing ANY message 

void BLEModule::setup() {
  SenseBoxBLE::start("senseBox-BLE");
}

bool BLEModule::begin() {
  int rxHandle = SenseBoxBLE::setConfigCharacteristic(BLE_SERVICE_UUID, BLE_RX_UUID);
  if (rxHandle <= 0) Serial.println("BLE: setConfigCharacteristic failed or already set.");
  SenseBoxBLE::configHandler = BLEModule::onBleConfigWrite;
  SenseBoxBLE::setName("senseBoxOS");
  SenseBoxBLE::advertise();
  return true;
}

void BLEModule::loop() {
  SenseBoxBLE::poll();
  bleMaybeFlushByIdle();
}

void BLEModule::bleFlush(const char* reason) {
  String s = bleBuf;
  s.trim();
  if (s.length() == 0) return;
  Serial.printf("[BLE] FULL (%s): \"%s\"\n", reason, s.c_str());

  // Case-insensitive control words
  String up = s; 
  up.toUpperCase();

  // clear buffer, so that its ready for a new command
  bleBuf = "";
  bleParenDepth = 0;
  bleSawOpenParen = false;

  if (up == "RUN") {
    runningScript = true; 
    runForever = false; 
    runScript();
    runningScript = false;
  }
  else if (up == "RUNLOOP") {
    runningScript = true; 
    runForever = true;  
    runScript();
  }
  else if (up == "STOP") {
    runForever = false; 
    runningScript = false;
    scriptLines.clear();
    variables.clear();
    Serial.println("Stopped");
  }
  else {
    scriptLines.push_back(s);
  }
}

void BLEModule::bleMaybeFlushByIdle() {
  if (bleBuf.length() == 0) return;
  if (bleParenDepth > 0)    return; // inside '(...' → wait for ')'
  uint32_t now = millis();
  if (now - lastBleByteMs >= BLE_IDLE_FLUSH_MS) {
    bleFlush("idle");
  }
}

String BLEModule::utf16leToString(uint8_t *data, size_t length)
{
    String result = "";

    // Ensure the length is even
    if (length % 2 != 0)
    {
        return result;
    }

    for (size_t i = 0; i < length; i += 2)
    {
        // Combine the two bytes to form a UTF-16 character
        uint16_t utf16_char = data[i] | (data[i + 1] << 8);

        // If the character is null (0x0000), break the loop as it indicates the end of the string
        if (utf16_char == 0x0000)
        {
            break;
        }

        // Append the character to the result string if it is within the ASCII range
        if (utf16_char <= 0x007F)
        {
            result += (char)utf16_char;
        }
    }

    return result;
}

void BLEModule::onBleConfigWrite() {
  uint8_t raw[64] = {0};
  SenseBoxBLE::read(raw, sizeof(raw));

  // Try to decode chunk (handles UTF-16LE & UTF-8)
  String chunk = BLEModule::utf16leToString(raw, sizeof(raw));
  
  // Show chunk with visible control characters
  Serial.print("[BLE] chunk (hex): ");
  for (size_t i = 0; i < chunk.length(); i++) {
    Serial.printf("%02X ", (unsigned char)chunk[i]);
  }
  Serial.println();
  Serial.printf("[BLE] chunk (string): \"%s\" (length: %d)\n", chunk.c_str(), chunk.length());

  // Feed decoded text into assembler
  bool justFlushed = false;
  for (size_t i = 0; i < (size_t)chunk.length(); ++i) {
    char c = chunk[i];
    Serial.printf("[BLE] Processing char[%d]: 0x%02X '%c'\n", i, (unsigned char)c, (c >= 32 && c < 127) ? c : '?');

    // ❗ Ignore all control characters (ASCII 0-31) — some apps send them
    if (c >= 0 && c < 32) {
      Serial.printf("[BLE] -> Skipping control character\n");
      continue;
    }

    // After a flush, ignore trailing whitespace and extra closing parens
    if (justFlushed && (c == ' ' || c == ')')) {
      Serial.printf("[BLE] -> Skipping trailing char after flush\n");
      continue;
    }
    justFlushed = false;

    // Accumulate
    bleBuf += c;
    Serial.printf("[BLE] -> Buffer now: \"%s\"\n", bleBuf.c_str());

    // Track parentheses to detect completion of calls like led(...)
    if (c == '(') { bleParenDepth++; bleSawOpenParen = true; }
    else if (c == ')') { if (bleParenDepth > 0) bleParenDepth--; }

    // IMMEDIATE flush when a function call closes cleanly
    if (c == ')' && bleParenDepth == 0 && bleSawOpenParen) {
      bleFlush("paren");
      justFlushed = true;
    }
  }

  Serial.printf("[BLE] After chunk processing, buffer: \"%s\"\n", bleBuf.c_str());

  // After processing chunk, check if buffer contains a control word
  if (bleParenDepth == 0 && !bleSawOpenParen && bleBuf.length() > 0) {
    String tmp = bleBuf;
    tmp.trim();
    String up = tmp;
    up.toUpperCase();
    if (up == "RUN" || up == "RUNLOOP" || up == "STOP") {
      Serial.printf("[BLE] Control word detected: %s\n", up.c_str());
      bleFlush("control");
    }
  }
  lastBleByteMs = millis(); // update “last seen” time for idle flush
}
