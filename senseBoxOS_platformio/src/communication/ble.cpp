#include "communication/ble.h"
#include "peripherals/display.h"

BLEModule bleModule;

// ===== BLE UUIDs =====
static const char* BLE_SERVICE_UUID = "CF06A218F68EE0BEAD048EBC1EB0BC84";
static const char* BLE_RX_UUID      = "2CDF217435BEFDC44CA26FD173F8B3A8";
String bleBuf; 
int  bleParenDepth = 0;
int  bleBraceDepth = 0;
bool bleSawOpenParen = false;
bool bleSawOpenBrace = false;
uint32_t lastBleByteMs = 0;
const uint32_t BLE_IDLE_FLUSH_MS = 800;  // idle gap before flushing ANY message

// Connection state tracking
static bool bleConnected = false;
static uint32_t lastActivityMs = 0;
const uint32_t BLE_DISCONNECT_TIMEOUT_MS = 5000;  // 5 seconds without activity = disconnected 

// List of known command prefixes
static const char* knownCommands[] = {
  "led(", "delay(", "display(", "clearDisplay(", "if(", "while(", "for(", "else", "}", 
  "sensor:", "buttonPressed(","lightBoard=sensor:board:light","airQuality = sensor:bme680:aiq",NULL
};

// Check if string starts with a known command
static bool startsWithKnownCommand(const String& line) {
  for (int i = 0; knownCommands[i] != NULL; i++) {
    if (line.startsWith(knownCommands[i])) {
      return true;
    }
  }
  return false;
}

// Helper to clean line from input artifacts like leading <> chars and garbage before commands
static String cleanLine(String line) {
  line.trim();
  // Remove leading < or > followed by space (likely encoding artifacts from some BLE apps)
  while (line.length() > 1 && (line[0] == '<' || line[0] == '>') && line[1] == ' ') {
    line = line.substring(2);
    line.trim();
  }
  
  // Fix for BLE garbage before commands: if line starts with 1-2 garbage chars 
  // followed by a known command, strip the garbage
  if (line.length() > 2 && !startsWithKnownCommand(line)) {
    // Try stripping 1 character
    String stripped1 = line.substring(1);
    if (startsWithKnownCommand(stripped1)) {
      Serial.printf("[BLE] Stripped garbage char '%c' from line\n", line[0]);
      return stripped1;
    }
    // Try stripping 2 characters
    if (line.length() > 3) {
      String stripped2 = line.substring(2);
      if (startsWithKnownCommand(stripped2)) {
        Serial.printf("[BLE] Stripped garbage chars '%s' from line\n", line.substring(0, 2).c_str());
        return stripped2;
      }
    }
  }
  
  return line;
}

void BLEModule::setup() {
  SenseBoxBLE::start("senseBox-BLE");
  delay(500);  // Wait for BLE to initialize
}

bool BLEModule::begin() {
  return begin(deviceID);
}

bool BLEModule::begin(String deviceId) {
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
  SenseBoxBLE::poll();
  bleMaybeFlushByIdle();
  checkConnectionState();
}

void BLEModule::bleFlush(const char* reason) {
  String s = bleBuf;
  s.trim();
  if (s.length() == 0) return;
  Serial.printf("[BLE] FULL (%s): \"%s\"\n", reason, s.c_str());
  
  // Mark activity for connection tracking (already marked in onBleConfigWrite)
  lastActivityMs = millis();

  // Case-insensitive control words
  String up = s; 
  up.toUpperCase();

  // clear buffer, so that its ready for a new command
  bleBuf = "";
  bleParenDepth = 0;
  bleBraceDepth = 0;
  bleSawOpenParen = false;
  bleSawOpenBrace = false;

  if (up == "RUN") {
    if (runningScript) {
      Serial.println("[BLE] Script already running, stopping current script first...");
      runningScript = false;
      runForever = false;
      delay(100); // Brief delay to allow current iteration to finish
      scriptLines.clear();
      variables.clear();
    }
    
    // Don't start if no script lines (prevents starting with empty script after stop)
    if (scriptLines.size() == 0) {
      Serial.println("[BLE] Ignoring RUN - no script lines available");
      return;
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
  }
  else if (up == "LOOP") {
    if (runningScript) {
      Serial.println("[BLE] Script already running, stopping current script first...");
      runningScript = false;
      runForever = false;
      delay(100); // Brief delay to allow current iteration to finish
      scriptLines.clear();
      variables.clear();
    }
    
    // Don't start if no script lines (prevents starting with empty script after stop)
    if (scriptLines.size() == 0) {
      Serial.println("[BLE] Ignoring LOOP - no script lines available");
      return;
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
  }
  else if (up == "STOP") {
    runForever = false; 
    runningScript = false;
    scriptLines.clear();
    variables.clear();
    Serial.println("Stopped");
  }
  else {
    // Ignore new script lines while a script is running to prevent corruption
    if (runningScript) {
      Serial.printf("[BLE] Ignoring line while script is running: \"%s\"\n", s.c_str());
      return;
    }
    
    // Split line at braces and commands to match expected interpreter format
    // e.g. "if(x){led(1,2,3)delay(100)}else{led(4,5,6)}" becomes multiple lines
    String remaining = s;
    
    // Debug: print hex dump of remaining to see if there are hidden characters
    Serial.print("[BLE] Parsing hex: ");
    for (int i = 0; i < remaining.length(); i++) {
      Serial.printf("%02X ", (unsigned char)remaining[i]);
    }
    Serial.println();
    
    int braceDepth = 0;
    int parenDepth = 0;
    String currentLine = "";
    
    for (int i = 0; i < remaining.length(); i++) {
      char c = remaining[i];
      Serial.printf("[BLE] Parse[%d]='%c'(0x%02X) depth=%d/%d line='%s'\n", 
                    i, (c >= 32 && c < 127) ? c : '?', (unsigned char)c, 
                    parenDepth, braceDepth, currentLine.c_str());
      
      if (c == '(') {
        parenDepth++;
        currentLine += c;
      }
      else if (c == ')') {
        parenDepth--;
        currentLine += c;
        
        // If we're inside braces and parens are balanced, end of a command
        if (braceDepth > 0 && parenDepth == 0) {
          String line = cleanLine(currentLine);
          if (line.length() > 0) {
            scriptLines.push_back(line);
            Serial.printf("[BLE] Added line: \"%s\"\n", line.c_str());
          }
          currentLine = "";
        }
      }
      else if (c == '{') {
        currentLine += c;
        braceDepth++;
        // Flush line with opening brace
        String line = cleanLine(currentLine);
        Serial.printf("[BLE] After '{': currentLine='%s', flushing\n", currentLine.c_str());
        if (line.length() > 0) {
          scriptLines.push_back(line);
          Serial.printf("[BLE] Added line: \"%s\"\n", line.c_str());
        }
        currentLine = "";
        Serial.printf("[BLE] After '{' reset: currentLine='%s'\n", currentLine.c_str());
      }
      else if (c == '}') {
        braceDepth--;
        // Flush any content before closing brace
        String line = cleanLine(currentLine);
        if (line.length() > 0) {
          scriptLines.push_back(line);
          Serial.printf("[BLE] Added line: \"%s\"\n", line.c_str());
        }
        scriptLines.push_back("}");
        Serial.printf("[BLE] Added line: \"}\"\n");
        currentLine = "";
      }
      else {
        currentLine += c;
      }
    }
    
    // Add any remaining content
    String line = cleanLine(currentLine);
    if (line.length() > 0) {
      scriptLines.push_back(line);
      Serial.printf("[BLE] Added line: \"%s\"\n", line.c_str());
    }
  }
}

void BLEModule::bleMaybeFlushByIdle() {
  if (bleBuf.length() == 0) return;
  if (bleParenDepth > 0)    return; // inside '(...' → wait for ')'
  if (bleBraceDepth > 0)    return; // inside '{...' → wait for '}'
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

    // Handle newline (\n or \r) as explicit line break/flush
    if (c == '\n' || c == '\r') {
      Serial.printf("[BLE] -> Newline received, flushing buffer\n");
      bleFlush("newline");
      bleBuf = "";
      bleParenDepth = 0;
      bleBraceDepth = 0;
      bleSawOpenParen = false;
      bleSawOpenBrace = false;
      continue;
    }
    // Ignore other control characters (ASCII 0-31)
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

    // Check if a keyword is starting after the last space
    // If so, flush what we have before the space
    if (bleBuf.length() > 0 && bleParenDepth == 0 && bleBraceDepth == 0) {
      int lastSpace = bleBuf.lastIndexOf(' ');
      if (lastSpace >= 0) {
        String afterSpace = bleBuf.substring(lastSpace + 1) + c;
        // Check if this forms the beginning of a keyword
        if (afterSpace == "f" || afterSpace == "fo" || afterSpace == "for" || 
            afterSpace == "i" || afterSpace == "if" || 
            afterSpace == "w" || afterSpace == "wh" || afterSpace == "whi" || afterSpace == "whil" || afterSpace == "while" || 
            afterSpace == "e" || afterSpace == "el" || afterSpace == "els" || afterSpace == "else") {
          // Keyword is forming after space, flush everything up to the space
          Serial.printf("[BLE] Keyword '%s' forming after space, flushing buffer first\n", afterSpace.c_str());
          String beforeSpace = bleBuf.substring(0, lastSpace);
          bleBuf = beforeSpace;
          bleFlush("pre-keyword");
          bleBuf = "";  // Clear for the new keyword
          justFlushed = true;
        }
      }
    }

    // Accumulate
    bleBuf += c;
    bleBuf.trim(); // Remove trailing whitespaces
    Serial.printf("[BLE] -> Buffer now: \"%s\"\n", bleBuf.c_str());

    // Track parentheses and braces to detect completion
    if (c == '(') { bleParenDepth++; bleSawOpenParen = true; }
    else if (c == ')') { if (bleParenDepth > 0) bleParenDepth--; }
    if (c == '{') { bleBraceDepth++; bleSawOpenBrace = true; }
    else if (c == '}') { if (bleBraceDepth > 0) bleBraceDepth--; }

    // Check if buffer starts with a control structure keyword
    bool isControlStructure = false;
    if (bleBuf.length() >= 2) {
      String bufTrimmed = bleBuf;
      bufTrimmed.trim();
      if (bufTrimmed.startsWith("if(") || 
          bufTrimmed.startsWith("while(") || 
          bufTrimmed.startsWith("for(") ||
          bufTrimmed.startsWith("else")) {
        isControlStructure = true;
      }
    }

    // IMMEDIATE flush when closing paren, but ONLY if:
    // - Not a control structure (if/while/for expect braces)
    // - No braces are currently open or expected
    if (c == ')' && bleParenDepth == 0 && bleSawOpenParen && 
        bleBraceDepth == 0 && !bleSawOpenBrace && !isControlStructure) {
      bleFlush("paren");
      justFlushed = true;
    }
    
    // Flush when closing brace and everything is closed
    if (c == '}' && bleBraceDepth == 0 && bleSawOpenBrace && bleParenDepth == 0) {
      bleFlush("brace");
      justFlushed = true;
    }

    // Check if buffer ends with a control word (only if not inside parens/braces)
    if (bleParenDepth == 0 && bleBraceDepth == 0 && bleBuf.length() >= 3) {
      // Get uppercase version to check for control words
      String bufUpper = bleBuf;
      bufUpper.toUpperCase();
      
      String controlWord = "";
      int controlWordLen = 0;
      
      if (bufUpper.endsWith("RUN")) {
        controlWord = "RUN";
        controlWordLen = 3;
      } else if (bufUpper.endsWith("LOOP")) {
        controlWord = "LOOP";
        controlWordLen = 4;
      } else if (bufUpper.endsWith("STOP")) {
        controlWord = "STOP";
        controlWordLen = 4;
      }
      
      if (controlWordLen > 0) {
        Serial.printf("[BLE] Control word '%s' detected at end of buffer\n", controlWord.c_str());
        
        // Extract everything before the control word
        String beforeControl = bleBuf.substring(0, bleBuf.length() - controlWordLen);
        beforeControl.trim();
        
        // Add what was before to script if not empty
        if (beforeControl.length() > 0) {
          Serial.printf("[BLE] Adding to script before control: \"%s\"\n", beforeControl.c_str());
          scriptLines.push_back(beforeControl);
        }
        
        // Now flush with just the control word
        bleBuf = controlWord;
        bleFlush("control");
        justFlushed = true;
      }
    }
  }

  Serial.printf("[BLE] After chunk processing, buffer: \"%s\"\n", bleBuf.c_str());
  lastBleByteMs = millis(); // update “last seen” time for idle flush
}
