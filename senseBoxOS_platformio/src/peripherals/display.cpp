#include "peripherals/display.h"
#include "logic/eval_var.h"

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
  Serial.print("Display: "); Serial.println(value);

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

void handleDisplay(String args) { 
  displayNumber(evalNumber(args)); 
}