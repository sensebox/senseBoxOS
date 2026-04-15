#include "peripherals/display.h"
#include "logic/eval.h"
#include "helpers/command_parser.h"
#include <SenseBoxBLE.h>

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledInitialized = false;

// Helper: Convert UTF-8 string to CP437 for Adafruit GFX display
// CP437 contains German umlauts: ä=0x84, Ä=0x8E, ö=0x94, Ö=0x99, ü=0x81, Ü=0x9A, ß=0xE1
String utf8ToCP437(const String& utf8) {
  String result = "";
  for (int i = 0; i < utf8.length(); i++) {
    uint8_t c = utf8[i];
    
    // Check for UTF-8 multi-byte sequences
    if ((c & 0x80) == 0) {
      // ASCII character (0x00-0x7F) - pass through
      result += (char)c;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < utf8.length()) {
      // 2-byte UTF-8 sequence
      uint8_t c2 = utf8[i + 1];
      uint16_t unicode = ((c & 0x1F) << 6) | (c2 & 0x3F);
      
      // Map common German characters to CP437
      switch (unicode) {
        case 0x00E4: result += (char)0x84; break; // ä
        case 0x00C4: result += (char)0x8E; break; // Ä
        case 0x00F6: result += (char)0x94; break; // ö
        case 0x00D6: result += (char)0x99; break; // Ö
        case 0x00FC: result += (char)0x81; break; // ü
        case 0x00DC: result += (char)0x9A; break; // Ü
        case 0x00DF: result += (char)0xE1; break; // ß
        case 0x00B0: result += (char)0xF8; break; // ° (degree symbol)
        default: result += '?'; break; // Unknown character
      }
      i++; // Skip next byte
    } else if ((c & 0xF0) == 0xE0 && i + 2 < utf8.length()) {
      // 3-byte UTF-8 sequence (we don't need these for German, but handle gracefully)
      result += '?';
      i += 2;
    } else {
      // Invalid or unsupported - use question mark
      result += '?';
    }
  }
  return result;
}

void initDisplay() {
  if (!oledInitialized) {
    if (oled.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      oledInitialized = true;
      oled.clearDisplay();
      oled.setTextSize(1);
      oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
      oled.setCursor(0,0);
      oled.display();
      Serial.println("OLED ready at 0x3D");
    } else {
      Serial.println("OLED not found at 0x3D (will retry on first display())");
    }
  }
}

void clearDisplay() {
  if (oledInitialized) {
    oled.clearDisplay();
      oled.display();

  }
}


static int displayTextY = 0;
void displayText(const String& text, uint8_t textSize) {
  initDisplay();
  oled.setTextSize(textSize);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.setCursor(0, displayTextY);
  oled.println(utf8ToCP437(text));
  oled.display();
}

void displayNumber(float value, uint8_t textSize, const String& unit) {
  initDisplay();
  oled.setTextSize(textSize);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.setCursor(0, displayTextY);
  
  // Smart formatting: show no decimals for whole numbers
  String out;
  if (value == (int)value) {
    out = String((int)value);
  } else {
    out = String(value);
  }
  if (unit.length() > 0) out += unit;
  oled.println(out);
  
  oled.display();

}

void resetDisplayTextY() {
  displayTextY = 0;
}

void handleClearDisplay(String args) {
  clearDisplay();
  resetDisplayTextY();
  // Update display only when explicitly clearing (user command)
  if (oledInitialized) {
    oled.display();
  }
}



// Returns the display unit for a known sensor measurement type
static String getSensorUnit(const String& expr) {
  // expr format: sensor:type:measurement
  int lastColon = expr.lastIndexOf(':');
  if (lastColon == -1) return "";
  String measurement = expr.substring(lastColon + 1);
  measurement.toLowerCase();
  if (measurement == "temperature") return String((char)0xF8) + "C"; // °C in CP437
  if (measurement == "humidity")    return "%";
  return "";
}

void handleDisplay(String args) {
  args.trim();

  // Parse optional size parameter: DISPLAY "text", S|M|L
  uint8_t textSize = 1; // default: S
  int commaPos = -1;

  // Find comma outside of quotes
  if (args.startsWith("\"")) {
    int closeQuote = args.indexOf('"', 1);
    if (closeQuote != -1) {
      commaPos = args.indexOf(',', closeQuote + 1);
    }
  } else {
    commaPos = args.indexOf(',');
  }

  if (commaPos != -1) {
    String sizeArg = args.substring(commaPos + 1);
    sizeArg.trim();
    sizeArg.toUpperCase();
    if (sizeArg == "M") textSize = 2;
    else if (sizeArg == "L") textSize = 3;
    args = args.substring(0, commaPos);
    args.trim();
  }

  // Increase y counter based on text size
  displayTextY += 8 * textSize + 2;

  if (args.startsWith("\"") && args.endsWith("\"")) {
    String inside = args.substring(1, args.length() - 1);
    displayText(inside, textSize);
    return;
  }
  // Ansonsten: Zahl evaluieren
  String unit = "";
  if (args.startsWith("sensor:")) unit = getSensorUnit(args);
  float num = evalNumber(args);
  displayNumber(num, textSize, unit);
}

// Global device ID variable
String deviceID = "";

String getDeviceID() {
  // Initialize global device ID if not set
  if (deviceID.length() == 0) {
    // Get MCU ID from BLE module and use last 4 characters
    String fullId = SenseBoxBLE::getMCUId();
    Serial.printf("Full MCU ID: %s\n", fullId.c_str());
    
    // Check if we got a valid ID (not "-1", not "0", and reasonable length)
    if (fullId.length() >= 4 && fullId != "-1" && fullId != "0") {
      deviceID = fullId.substring(fullId.length() - 4);
    } else if (fullId.length() > 0 && fullId.length() < 4 && fullId != "-1" && fullId != "0") {
      deviceID = fullId;  // Use full ID if less than 4 chars but valid
    } else {
      // BLE not available, use fallback ID
      deviceID = "NBEE";  // No BLE
      Serial.println("BLE not available - using fallback device ID");
    }
    
    Serial.printf("Device ID: %s\n", deviceID.c_str());
  }
  
  return deviceID;
}



const unsigned char bluetooth_icon[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x18, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x16, 0x00, 0x00,
  0x13, 0x00, 0x00, 0x11, 0x80, 0x01, 0x10, 0x80, 0x00, 0x91, 0x00, 0x00, 0x52, 0x00, 0x00, 0x34,
  0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x34, 0x00, 0x00, 0x52, 0x00, 0x00, 0x91, 0x00,
  0x01, 0x10, 0x80, 0x00, 0x11, 0x80, 0x00, 0x13, 0x00, 0x00, 0x16, 0x00, 0x00, 0x1C, 0x00, 0x00,
  0x18, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
};
void displayDeviceID() {
  if (!oledInitialized) {
    if (oled.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      oledInitialized = true;
    } else {
      return;
    }
  }

  String deviceID = getDeviceID();
  
  oled.clearDisplay();
  
  // Draw Bluetooth icon centered (128-24)/2 = 52
  oled.drawBitmap(52, 8, bluetooth_icon, 24, 24, SSD1306_WHITE);
  
  // Device ID below icon - centered
  oled.setTextSize(2);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  int16_t x1, y1;
  uint16_t w, h;
  oled.getTextBounds(deviceID, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((128 - w) / 2, 36);
  oled.println(deviceID);
  
  // German instruction text at bottom - centered
  oled.setTextSize(1);
  String instruction = utf8ToCP437("Verbinde Basic App");
  oled.getTextBounds(instruction, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((128 - w) / 2, 54);
  oled.println(instruction);

  // Version top-right corner
  oled.setTextSize(1);
  String version = "v" SENSEBOX_OS_VERSION;
  oled.getTextBounds(version, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor(128 - w, 0);
  oled.println(version);
  
  oled.display();
}

void displaySerialOnlyMode() {
  if (!oledInitialized) {
    if (oled.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      oledInitialized = true;
    } else {
      return;
    }
  }

  oled.clearDisplay();
  
  // Title
  oled.setTextSize(2);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  int16_t x1, y1;
  uint16_t w, h;
  String title = "Blockly";
  oled.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((128 - w) / 2, 8);
  oled.println(title);
  
  // Instructions - centered
  oled.setTextSize(1);
  String line1 = utf8ToCP437("Verbinde dich");
  String line2 = utf8ToCP437("per PC (USB)");
  
  oled.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((128 - w) / 2, 36);
  oled.println(line1);
  
  oled.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((128 - w) / 2, 46);
  oled.println(line2);

  // Version top-right corner
  oled.setTextSize(1);
  String version = "v" SENSEBOX_OS_VERSION;
  oled.getTextBounds(version, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor(128 - w, 0);
  oled.println(version);
  
  oled.display();
}

void displayMeasurement(float value, const String& sensorName, const String& unit, int decimals) {
  initDisplay();
  oled.clearDisplay();
  
  // Format value with requested decimal places
  // Smart formatting: if value is a whole number and decimals=0, show without decimals
  String valueStr;
  if (decimals == 0) {
    // For integer display mode, truncate decimals (don't round)
    valueStr = String((int)value);
  } else if (value == (int)value) {
    // If value is whole number, show without decimals regardless of decimals setting
    valueStr = String((int)value);
  } else {
    valueStr = String(value, decimals);
  }
  String valueWithUnit = valueStr + " " + utf8ToCP437(unit);
  
  // Calculate dimensions for value+unit (larger text, size 2)
  oled.setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  oled.getTextBounds(valueWithUnit, 0, 0, &x1, &y1, &w, &h);
  
  // Display value and unit centered at top
  int valueX = (SCREEN_WIDTH - w) / 2;
  int valueY = 12;
  oled.setCursor(valueX, valueY);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.println(valueWithUnit);
  
  // Calculate dimensions for sensor name (smaller text, size 1)
  oled.setTextSize(1);
  String convertedName = utf8ToCP437(sensorName);
  oled.getTextBounds(convertedName, 0, 0, &x1, &y1, &w, &h);
  
  // Display sensor name centered below value
  int nameX = (SCREEN_WIDTH - w) / 2;
  int nameY = valueY + 20;  // 20 pixels below value
  oled.setCursor(nameX, nameY);
  oled.println(convertedName);
  
  oled.display();
}

void handleDisplayMeasurement(String args) {
  // Parse format: displayMeasurement(sensor:bme680:temperature, "Temperatur", "°C")
  args.trim();
  
  // Find comma positions
  int firstComma = args.indexOf(',');
  if (firstComma == -1) {
    Serial.println("Error: displayMeasurement requires 3 arguments: value, name, unit");
    return;
  }
  
  int secondComma = args.indexOf(',', firstComma + 1);
  if (secondComma == -1) {
    Serial.println("Error: displayMeasurement requires 3 arguments: value, name, unit");
    return;
  }
  
  // Extract parts
  String valuePart = args.substring(0, firstComma);
  String namePart = args.substring(firstComma + 1, secondComma);
  String unitPart = args.substring(secondComma + 1);
  
  // Clean up whitespace
  valuePart.trim();
  namePart.trim();
  unitPart.trim();
  
  // Extract string literals (remove quotes)
  if (namePart.startsWith("\"") && namePart.endsWith("\"")) {
    namePart = namePart.substring(1, namePart.length() - 1);
  }
  
  if (unitPart.startsWith("\"") && unitPart.endsWith("\"")) {
    unitPart = unitPart.substring(1, unitPart.length() - 1);
  }
  
  // Decide decimal places based on sensor/measurement (if it's a sensor command)
  int decimals = 2;
  if (isSensorCommand(valuePart)) {
    SensorCommand cmd = parseSensorCommand(valuePart);
    if (cmd.isValid) {
      String m = cmd.measurement;
      m.toLowerCase();
      if (m.equalsIgnoreCase("temperature")) decimals = 1;
      else if (m.equalsIgnoreCase("iaq")) decimals = 0;
      else if (m.equalsIgnoreCase("humidity")) decimals = 0;
      else if (m.equalsIgnoreCase("brightness") || m.equalsIgnoreCase("light") || m.equalsIgnoreCase("lux")) decimals = 0;
    }
  } else {
    // Try to infer from unit string: Celsius -> 1 decimal, percent -> 0
    if (unitPart.indexOf("°C") != -1 || unitPart.indexOf("C") != -1) decimals = 1;
    else if (unitPart.indexOf("%") != -1) decimals = 0;
  }

  // Evaluate the value (could be sensor reading or numeric expression)
  float value = evalNumber(valuePart);

  // Display the measurement with chosen precision
  displayMeasurement(value, namePart, unitPart, decimals);
}
