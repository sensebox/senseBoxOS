#include "communication/ble.h"
#include "peripherals/display.h"
#include "communication/protocol.h"

BLEModule bleModule;

// ===== BLE UUIDs =====
static const char* BLE_SERVICE_UUID = "CF06A218F68EE0BEAD048EBC1EB0BC84";
static const char* BLE_RX_UUID      = "2CDF217435BEFDC44CA26FD173F8B3A8";

// Connection state tracking
static bool bleConnected = false;
static uint32_t lastActivityMs = 0;

void BLEModule::setup() {
  Serial.println("[BLE] Attempting to initialize BLE...");
  
  // Try quick detection first with minimal delay
  SenseBoxBLE::start("senseBox-BLE");
  delay(100);  // Short initial delay
  
  // Try to get MCU ID to verify BLE is working
  String testId = SenseBoxBLE::getMCUId();
  
  // If first attempt fails, try once more with slightly longer delay
  if (testId.length() == 0 || testId == "-1" || testId == "0") {
    delay(200);
    testId = SenseBoxBLE::getMCUId();
  }
  
  // Check if we got a valid ID (not empty, not "-1", and reasonable length)
  if (testId.length() > 0 && testId != "-1" && testId != "0" && testId.length() > 2) {
    bleAvailable = true;
    Serial.println("[BLE] BLE module detected and initialized");
  } else {
    bleAvailable = false;
    Serial.printf("[BLE] BLE module not detected (ID: '%s') - BLE features disabled\n", testId.c_str());
  }
}

bool BLEModule::begin() {
  if (!bleAvailable) {
    Serial.println("[BLE] BLE not available, skipping begin()");
    return false;
  }
  return begin(deviceID);
}

bool BLEModule::begin(String deviceId) {
  if (!bleAvailable) {
    Serial.println("[BLE] BLE not available, skipping begin()");
    return false;
  }
  
  int rxHandle = SenseBoxBLE::setConfigCharacteristic(BLE_SERVICE_UUID, BLE_RX_UUID);
  if (rxHandle <= 0) Serial.println("BLE: setConfigCharacteristic failed or already set.");
  SenseBoxBLE::configHandler = BLEModule::onBleConfigWrite;
  
  // Use provided device ID as BLE name
  String deviceName = "senseBox-" + deviceId;
  SenseBoxBLE::setName(deviceName);
  Serial.printf("BLE device name: %s\n", deviceName.c_str());
  
  SenseBoxBLE::advertise();
  return true;
}

void BLEModule::loop() {
  if (!bleAvailable) return;  // Skip polling if BLE not available
  
  SenseBoxBLE::poll();
  commandBuffer.checkIdleFlush();  // Use central buffer's idle flush
  checkConnectionState();
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

void BLEModule::checkConnectionState() {
  // Connection state is now only managed by actual data reception
  // Once connected, the device stays connected (no idle timeout)
  // The welcome screen will only show again on device reboot
}

void BLEModule::onBleConfigWrite() {
  uint8_t raw[64] = {0};
  SenseBoxBLE::read(raw, sizeof(raw));

  // Mark activity for connection tracking
  lastActivityMs = millis();
  
  // First connection - clear display immediately on first data
  if (!bleConnected) {
    bleConnected = true;
    if (oledInitialized) {
      oled.clearDisplay();
      oled.display();
    }
    Serial.println("[BLE] Device connected - display cleared");
  }

  // Try to decode chunk (handles UTF-16LE & UTF-8)
  String chunk = BLEModule::utf16leToString(raw, sizeof(raw));
  
  // Debug output
  Serial.print("[BLE] chunk (hex): ");
  for (size_t i = 0; i < chunk.length(); i++) {
    Serial.printf("%02X ", (unsigned char)chunk[i]);
  }
  Serial.println();
  Serial.printf("[BLE] chunk (string): \"%s\" (length: %d)\n", chunk.c_str(), chunk.length());

  // Remove trailing space from chunk (fix for sender-side chunking bug)
  // The sender app adds spaces between 13-char chunks
  if (chunk.length() > 0 && chunk[chunk.length() - 1] == ' ') {
    chunk.remove(chunk.length() - 1);
    Serial.printf("[BLE] Removed trailing space, new length: %d\n", chunk.length());
  }

  // Feed decoded text into central command buffer
  commandBuffer.processString(chunk);
}
