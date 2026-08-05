#include "peripherals/display.h"
#include "logic/eval.h"
#include "logic/var.h"
#include "helpers/command_parser.h"
#include <SenseBoxBLE.h>

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledInitialized = false;



// Helper: Convert UTF-8 string to CP437 for Adafruit GFX display
// CP437 contains German umlauts: ä=0x84, Ä=0x8E, ö=0x94, Ö=0x99, ü=0x81, Ü=0x9A, ß=0xE1
// Supports /xHH escape sequences (e.g. /x84 → CP437 byte 0x84 = ä)
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

void displayNumber(float value, uint8_t textSize) {
  initDisplay();
  oled.setTextSize(textSize);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.setCursor(0, displayTextY);
  
  // Smart formatting: show no decimals for whole numbers
  String out;
  if (value == (int)value) {
    oled.println((int)value);
  } else {
    oled.println(value);
  }

  
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





void handleDisplay(String args) {
  args.trim();
  Serial.printf("[DISPLAY] Raw args: '%s'\n", args.c_str());

  // Parse optional size parameter: DISPLAY "text", S|M|L
  uint8_t textSize = 1; // default: S
  int commaPos = -1;

  // Find comma outside of quotes and parentheses
  if (args.startsWith("\"")) {
    int closeQuote = args.indexOf('"', 1);
    if (closeQuote != -1) {
      commaPos = args.indexOf(',', closeQuote + 1);
    }
  } else {
    // Find comma at top level (not inside parentheses)
    int depth = 0;
    for (int i = 0; i < args.length(); i++) {
      if (args[i] == '(') depth++;
      else if (args[i] == ')') depth--;
      else if (args[i] == ',' && depth == 0) {
        commaPos = i;
        break;
      }
    }
  }

  if (commaPos != -1) {
    String sizeArg = args.substring(commaPos + 1);
    sizeArg.trim();
    sizeArg.toUpperCase();
    Serial.printf("[DISPLAY] Size arg: '%s'\n", sizeArg.c_str());
    if (sizeArg == "M") textSize = 2;
    else if (sizeArg == "L") textSize = 3;
    args = args.substring(0, commaPos);
    args.trim();
    Serial.printf("[DISPLAY] After parsing size, args: '%s'\n", args.c_str());
  }

  // Increase y counter based on text size
  displayTextY += 8 * textSize + 2;

  if (args.startsWith("\"") && args.endsWith("\"")) {
    String inside = args.substring(1, args.length() - 1);
    displayText(inside, textSize);
    return;
  }
  
  // Check if it's a string variable
  if (isVarString(args)) {
    String strValue = getVarString(args);
    Serial.printf("[DISPLAY] Variable '%s' is string: \"%s\"\n", args.c_str(), strValue.c_str());
    displayText(strValue, textSize);
    return;
  }
  
  // Otherwise: evaluate as number
  float num = evalNumber(args);
  displayNumber((float)num, textSize);  
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

// Pixel drawing function
void drawPixel(int x, int y, uint16_t color) {
  if (!oledInitialized) {
    initDisplay();
  }
  
  // Validate coordinates
  if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
    Serial.printf("[DRAWPIXEL] Error: Pixel outside bounds (%d, %d). Display is %dx%d\n", 
                  x, y, SCREEN_WIDTH, SCREEN_HEIGHT);
    return;
  }
  
  // Draw the pixel
  oled.drawPixel(x, y, color);
}

// Refresh/update display after drawing pixels
void refreshDisplay() {
  if (oledInitialized) {
    oled.display();
  }
}

// Handle DRAWPIXEL command
// Format: DRAWPIXEL x, y [, color]
// Examples: DRAWPIXEL 64, 32
//           DRAWPIXEL 10, 20, 1 (color: 1=white, 0=black)
void handleDrawPixel(String args) {
  args.trim();
  
  // Parse comma-separated values
  int firstComma = args.indexOf(',');
  if (firstComma == -1) {
    Serial.println("[DRAWPIXEL] Error: Expected format: DRAWPIXEL x, y [, color]");
    return;
  }
  
  int secondComma = args.indexOf(',', firstComma + 1);
  
  String xStr = args.substring(0, firstComma);
  String yStr;
  String colorStr = "1"; // default: white
  
  if (secondComma == -1) {
    // Only x and y provided
    yStr = args.substring(firstComma + 1);
  } else {
    // x, y, and color provided
    yStr = args.substring(firstComma + 1, secondComma);
    colorStr = args.substring(secondComma + 1);
  }
  
  xStr.trim();
  yStr.trim();
  colorStr.trim();
  
  // Parse coordinates
  int x = evalNumber(xStr);
  int y = evalNumber(yStr);
  uint16_t color = (evalNumber(colorStr) != 0) ? SSD1306_WHITE : SSD1306_BLACK;
  
  Serial.printf("[DRAWPIXEL] Drawing pixel at (%d, %d) with color %d\n", x, y, color);
  
  // Draw the pixel
  drawPixel(x, y, color);
}

// ============================================================================
// BITMAP SYSTEM
// ============================================================================

// Example bitmaps
// 8x8 Smiley face
const unsigned char bitmap_smiley[] PROGMEM = {
  0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C
};

// 8x8 Heart
const unsigned char bitmap_heart[] PROGMEM = {
  0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18
};

// 8x8 Warning/Exclamation
const unsigned char bitmap_warning[] PROGMEM = {
  0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18
};

// Bitmap registry (map of name -> bitmap)
#include <map>
std::map<String, Bitmap> bitmapRegistry;

// Register a bitmap for later use
void registerBitmap(const String& name, const Bitmap& bitmap) {
  bitmapRegistry[name] = bitmap;
  Serial.printf("[BITMAP] Registered bitmap '%s' (%d x %d)\n", name.c_str(), bitmap.width, bitmap.height);
}

// Get a bitmap by name
Bitmap* getBitmap(const String& name) {
  auto it = bitmapRegistry.find(name);
  if (it != bitmapRegistry.end()) {
    return &(it->second);
  }
  return nullptr;
}

// Draw a bitmap at specified position
void drawBitmap(int x, int y, const Bitmap& bitmap, uint16_t color) {
  if (!oledInitialized) {
    initDisplay();
  }
  
  // Validate position (allow drawing slightly outside if partial overlap)
  if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) {
    Serial.printf("[BITMAP] Warning: Bitmap position (%d, %d) outside display bounds\n", x, y);
  }
  
  oled.drawBitmap(x, y, bitmap.data, bitmap.width, bitmap.height, color);
  Serial.printf("[BITMAP] Drew bitmap (%d x %d) at position (%d, %d)\n", 
                bitmap.width, bitmap.height, x, y);
}

// Handle DRAWBITMAP command
// Format: DRAWBITMAP x, y, bitmap_name [, color]
// Examples: DRAWBITMAP 60, 28, smiley
//           DRAWBITMAP 10, 10, heart, 1
void handleDrawBitmap(String args) {
  args.trim();
  
  // Find first two commas
  int firstComma = args.indexOf(',');
  if (firstComma == -1) {
    Serial.println("[DRAWBITMAP] Error: Expected format: DRAWBITMAP x, y, bitmap_name [, color]");
    return;
  }
  
  int secondComma = args.indexOf(',', firstComma + 1);
  if (secondComma == -1) {
    Serial.println("[DRAWBITMAP] Error: Expected format: DRAWBITMAP x, y, bitmap_name [, color]");
    return;
  }
  
  int thirdComma = args.indexOf(',', secondComma + 1);
  
  // Extract parts
  String xStr = args.substring(0, firstComma);
  String yStr = args.substring(firstComma + 1, secondComma);
  String bitmapName;
  String colorStr = "1"; // default: white
  
  if (thirdComma == -1) {
    // Only x, y, and name provided
    bitmapName = args.substring(secondComma + 1);
  } else {
    // x, y, name, and color provided
    bitmapName = args.substring(secondComma + 1, thirdComma);
    colorStr = args.substring(thirdComma + 1);
  }
  
  xStr.trim();
  yStr.trim();
  bitmapName.trim();
  colorStr.trim();
  
  // Remove quotes from bitmap name if present
  if (bitmapName.startsWith("\"") && bitmapName.endsWith("\"")) {
    bitmapName = bitmapName.substring(1, bitmapName.length() - 1);
  }
  
  // Parse coordinates and color
  int x = evalNumber(xStr);
  int y = evalNumber(yStr);
  uint16_t color = (evalNumber(colorStr) != 0) ? SSD1306_WHITE : SSD1306_BLACK;
  
  // Look up bitmap
  Bitmap* bitmap = getBitmap(bitmapName);
  if (!bitmap) {
    Serial.printf("[DRAWBITMAP] Error: Bitmap '%s' not found\n", bitmapName.c_str());
    Serial.println("[DRAWBITMAP] Available bitmaps:");
    for (auto& pair : bitmapRegistry) {
      Serial.printf("  - %s (%d x %d)\n", pair.first.c_str(), pair.second.width, pair.second.height);
    }
    return;
  }
  
  Serial.printf("[DRAWBITMAP] Drawing bitmap '%s' at (%d, %d)\n", bitmapName.c_str(), x, y);
  drawBitmap(x, y, *bitmap, color);
}

// Initialize built-in bitmaps (call this on startup)
void initBitmaps() {
  Bitmap smiley = {bitmap_smiley, 8, 8};
  Bitmap heart = {bitmap_heart, 8, 8};
  Bitmap warning = {bitmap_warning, 8, 8};
  
  registerBitmap("smiley", smiley);
  registerBitmap("heart", heart);
  registerBitmap("warning", warning);
  
  Serial.println("[BITMAP] Built-in bitmaps initialized");
}

// ============================================================================
// DISPLAY MATRIX SYSTEM (8x8 grid, 16x8 pixels per cell)
// ============================================================================

// Draw an 8x8 matrix on the display
// Each cell is 16x8 pixels (total 128x64)
// matrix[row][col]: 0 = black, 1 = white
void displayMatrix(const uint8_t matrix[MATRIX_HEIGHT][MATRIX_WIDTH], bool showGrid) {
  if (!oledInitialized) {
    initDisplay();
  }
  
  oled.clearDisplay();
  
  // Draw each cell
  for (int row = 0; row < MATRIX_HEIGHT; row++) {
    for (int col = 0; col < MATRIX_WIDTH; col++) {
      int x = col * CELL_WIDTH;
      int y = row * CELL_HEIGHT;
      uint8_t value = matrix[row][col];
      
      // Draw filled rectangle or empty rectangle based on value
      if (value == 1) {
        // Draw filled white rectangle
        oled.fillRect(x, y, CELL_WIDTH, CELL_HEIGHT, SSD1306_WHITE);
      } else if (showGrid) {
        // Draw black rectangle with white border (grid mode)
        oled.drawRect(x, y, CELL_WIDTH, CELL_HEIGHT, SSD1306_WHITE);
      }
      // Otherwise: draw nothing (black on black)
    }
  }
  
  // Draw grid lines if requested
  if (showGrid) {
    // Vertical lines
    for (int col = 1; col < MATRIX_WIDTH; col++) {
      int x = col * CELL_WIDTH;
      oled.drawLine(x, 0, x, SCREEN_HEIGHT, SSD1306_WHITE);
    }
    // Horizontal lines
    for (int row = 1; row < MATRIX_HEIGHT; row++) {
      int y = row * CELL_HEIGHT;
      oled.drawLine(0, y, SCREEN_WIDTH, y, SSD1306_WHITE);
    }
  }
  
  oled.display();
  Serial.println("[MATRIX] Display matrix rendered");
}

// Parse and display matrix from string
// Format 1: DISPLAYMATRIX "10101010|01010101|10101010|01010101|10101010|01010101|10101010|01010101"
// Format 2: DISPLAYMATRIX 10101010 01010101 10101010 01010101 10101010 01010101 10101010 01010101
// (8 rows, each with 8 digits 0 or 1)
void handleDisplayMatrix(String args) {
  args.trim();
  
  uint8_t matrix[MATRIX_HEIGHT][MATRIX_WIDTH];
  bool showGrid = false;
  
  // Check if grid mode is requested (ends with ", grid" or similar)
  if (args.endsWith(", grid") || args.endsWith(",grid")) {
    showGrid = true;
    int commaPos = args.lastIndexOf(',');
    args = args.substring(0, commaPos);
    args.trim();
  }
  
  // Remove quotes if present
  if (args.startsWith("\"") && args.endsWith("\"")) {
    args = args.substring(1, args.length() - 1);
  }
  
  // Detect separator: pipe (|) or space
  char separator = ' ';
  if (args.indexOf('|') != -1) {
    separator = '|';
  }
  
  // Parse rows
  int rowIndex = 0;
  int startPos = 0;
  
  while (rowIndex < MATRIX_HEIGHT) {
    int endPos = args.indexOf(separator, startPos);
    if (endPos == -1) {
      endPos = args.length();
    }
    
    String rowStr = args.substring(startPos, endPos);
    rowStr.trim();
    
    if (rowStr.length() == MATRIX_WIDTH) {
      // Parse each digit in the row
      for (int col = 0; col < MATRIX_WIDTH; col++) {
        char digit = rowStr[col];
        if (digit == '1') {
          matrix[rowIndex][col] = 1;
        } else if (digit == '0') {
          matrix[rowIndex][col] = 0;
        } else {
          Serial.printf("[MATRIX] Error: Invalid character '%c' at row %d, col %d (expected 0 or 1)\n", 
                       digit, rowIndex, col);
          return;
        }
      }
      rowIndex++;
    } else if (rowStr.length() > 0) {
      Serial.printf("[MATRIX] Error: Row %d has length %d, expected %d\n", 
                   rowIndex, rowStr.length(), MATRIX_WIDTH);
      return;
    }
    
    startPos = endPos + 1;
    
    // Safety check to avoid infinite loop
    if (endPos >= args.length()) break;
  }
  
  if (rowIndex != MATRIX_HEIGHT) {
    Serial.printf("[MATRIX] Error: Expected %d rows, got %d\n", MATRIX_HEIGHT, rowIndex);
    return;
  }
  
  Serial.printf("[MATRIX] Matrix parsed successfully (grid mode: %s)\n", 
               showGrid ? "ON" : "OFF");
  displayMatrix(matrix, showGrid);
}
