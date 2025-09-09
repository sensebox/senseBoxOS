#include "peripherals/led.h"
#include "logic/eval_var.h"

Adafruit_NeoPixel rgb_led_1 = Adafruit_NeoPixel(1, 1, NEO_GRB + NEO_KHZ800);

void initLedRGB() {
  rgb_led_1.begin();
  rgb_led_1.setBrightness(30);
  rgb_led_1.show();
}

void setLedRGB(int r, int g, int b) {
  rgb_led_1.setPixelColor(0, rgb_led_1.Color(r, g, b));
  rgb_led_1.show();
  Serial.printf("LED RGB set to (%d,%d,%d)\n", r, g, b);
}

void handleLed(String args) {
  args.trim();
  if (args.indexOf(',') != -1) {
    int c1 = args.indexOf(',');
    int c2 = args.indexOf(',', c1 + 1);
    int r = (int)evalNumber(args.substring(0, c1));
    int g = (int)evalNumber(args.substring(c1 + 1, c2));
    int b = (int)evalNumber(args.substring(c2 + 1));
    setLedRGB(r, g, b);
  } else {
    args.toLowerCase();
    if (args == "on") setLedRGB(255, 0, 0);
    else if (args == "off") setLedRGB(0, 0, 0);
    else Serial.println("LED expects 'on', 'off' or 'r,g,b'");
  }
}