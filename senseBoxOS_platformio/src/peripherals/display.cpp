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

void displayNumber(float value) {

  if (!oledInitialized) {
    if (oled.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      oledInitialized = true;
      oled.clearDisplay();
      oled.setTextSize(1);
      oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
      oled.setCursor(0,0);
      oled.display();
      Serial.println("OLED initialized at 0x3D");
    } else {
      Serial.println("OLED init failed (0x3D)");
      return;
    }
  }

  oled.clearDisplay();
  oled.setCursor(0,0);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.println(value);
  oled.display();
}
void displayText(const String& text) {
  if (!oledInitialized) {
    if (oled.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      oledInitialized = true;
    } else {
      return;
    }
  }

  oled.clearDisplay();
  oled.setCursor(0,0);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  oled.println(text);
  oled.display();

}



void handleDisplay(String args) {

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
