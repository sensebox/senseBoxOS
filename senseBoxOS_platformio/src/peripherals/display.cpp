#include "peripherals/display.h"
#include "logic/eval.h"
#include <SenseBoxBLE.h>

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledInitialized = false;

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
void displayText(const String& text) {
  initDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.setCursor(0, displayTextY);
  oled.println(text);
  oled.display();
}

void displayNumber(float value) {
  initDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.setCursor(0, displayTextY);
  oled.println(value);
  oled.display();

}

void resetDisplayTextY() {
  displayTextY = 0;
}

void handleClearDisplay(String args) {
  clearDisplay();
  resetDisplayTextY();
}



void handleDisplay(String args) {
  // increae y counter every time we call handleDisplay
  displayTextY += 14;
  args.trim();
  if (args.startsWith("\"") && args.endsWith("\"")) {
    String inside = args.substring(1, args.length() - 1);
    displayText(inside);
    return;
  }
  // Ansonsten: Zahl evaluieren
  float num = evalNumber(args);
  displayNumber(num);


}

// Global device ID variable
String deviceID = "";

String getDeviceID() {
  // Initialize global device ID if not set
  if (deviceID.length() == 0) {
    // Get MCU ID from BLE module and use last 4 characters
    String fullId = SenseBoxBLE::getMCUId();
    Serial.printf("Full MCU ID: %s\n", fullId.c_str());
    
    if (fullId.length() >= 4) {
      deviceID = fullId.substring(fullId.length() - 4);
    } else {
      deviceID = fullId;  // Use full ID if less than 4 chars
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
  String instruction = "Verbinde Basic App";
  oled.getTextBounds(instruction, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((128 - w) / 2, 54);
  oled.println(instruction);
  
  oled.display();
}

void displayMeasurement(float value, const String& sensorName, const String& unit) {
  initDisplay();
  oled.clearDisplay();
  
  // Format value with 2 decimal places
  String valueStr = String(value, 2);
  String valueWithUnit = valueStr + " " + unit;
  
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
  oled.getTextBounds(sensorName, 0, 0, &x1, &y1, &w, &h);
  
  // Display sensor name centered below value
  int nameX = (SCREEN_WIDTH - w) / 2;
  int nameY = valueY + 20;  // 20 pixels below value
  oled.setCursor(nameX, nameY);
  oled.println(sensorName);
  
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
  
  // Evaluate the value (could be sensor reading or numeric expression)
  float value = evalNumber(valuePart);
  
  // Display the measurement
  displayMeasurement(value, namePart, unitPart);
}
