// ESP32-S2 Pseudocode Interpreter + OLED + RGB + SenseBoxBLE
// Robust BLE input: supports UTF-8 and UTF-16LE (ASCII subset), assembles across tiny writes,
// ignores CR/LF while typing, flushes on balanced ')' or idle gap, and prints completed lines.
//
// GATT UUIDs:
//   Service: CF06A218F68EE0BEAD048EBC1EB0BC84
//   RX Char: 2CDF217435BEFDC44CA26FD173F8B3A8

#include <Arduino.h>
#include <map>
#include <vector>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_HDC1000.h>
#include <SenseBoxBLE.h>

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledInitialized = false;

// ===== NeoPixel & Sensor =====
Adafruit_NeoPixel rgb_led_1 = Adafruit_NeoPixel(1, 1, NEO_GRB + NEO_KHZ800);
Adafruit_HDC1000 hdc;
bool hdcInitialized = false;

// ===== BLE UUIDs =====
static const char* BLE_SERVICE_UUID = "CF06A218F68EE0BEAD048EBC1EB0BC84";
static const char* BLE_RX_UUID      = "2CDF217435BEFDC44CA26FD173F8B3A8";

// ===== Interpreter State =====
std::map<String, float> variables;
std::vector<String> scriptLines;
bool runningScript = false;
bool runForever   = false;

String usbBuf;      // USB line assembly

// --- BLE line assembly/state ---
String bleBuf;                           // decoded text buffer (ASCII/UTF-8)
uint32_t lastBleByteMs = 0;              // last time we saw any BLE data
const uint32_t BLE_IDLE_FLUSH_MS = 800;  // idle gap before flushing ANY message
int  bleParenDepth = 0;
bool bleSawOpenParen = false;

// ===== Prototypes =====
float evalNumber(String expr);
bool  evalCond(String cond);
void  setVar(String name, float value);
float getVar(String name);
void  displayNumber(float value);
float readSensor();
void  setLedRGB(int r, int g, int b);
void  executeLine(String line, int& pc);
void  runScript();
void  processCompleteLine(const String& line, const char* tag);
void  pumpUartSource(Stream& in, String& buf, const char* tag);
bool  pumpControl();
void  onBleConfigWrite();
inline void bleMaybeFlushByIdle();
inline void bleFlush(const char* reason);

// ===== UTF-16LE support (ASCII subset) =====
String utf16leToAsciiString(const uint8_t* data, size_t length) {
  String result;
  if (length < 2) return result;
  if (length % 2 != 0) length--;
  for (size_t i = 0; i + 1 < length; i += 2) {
    uint16_t ch = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
    if (ch == 0x0000) break;              // NUL terminator
    if (ch <= 0x007F) result += (char)ch; // ASCII only (our commands)
  }
  return result;
}

// Heuristic decoder: detect UTF-16LE (0x00 high bytes) else treat as UTF-8/ASCII
String decodeBlePayload(const uint8_t* buf, size_t len) {
  int asciiPairs = 0, pairs = 0;
  for (size_t i = 0; i + 1 < len; i += 2) {
    if (buf[i] != 0x00 || buf[i + 1] != 0x00) {
      pairs++;
      if (buf[i] != 0x00 && buf[i + 1] == 0x00) asciiPairs++;
    }
  }
  if (pairs > 0 && (float)asciiPairs / (float)pairs > 0.6f) {
    return utf16leToAsciiString(buf, len);
  }
  String s;
  for (size_t i = 0; i < len; ++i) {
    if (buf[i] == 0x00) break;
    s += (char)buf[i];
  }
  return s;
}

// ===== Command Dispatch =====
typedef void (*CommandHandler)(String args);
std::map<String, CommandHandler> commandMap;

void handleDisplay(String args) { displayNumber(evalNumber(args)); }

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

void handleDelay(String args) {
  args.trim();
  long ms = (long)evalNumber(args);
  if (ms < 0) ms = 0;
  uint32_t start = millis();
  while (runningScript && (millis() - start < (uint32_t)ms)) {
    pumpControl();
    delay(1);
    yield();
  }
}

void setupCommandMap() {
  commandMap["display"] = handleDisplay;
  commandMap["led"]     = handleLed;
  commandMap["delay"]   = handleDelay;
}

// ===== Variables / Eval =====
void setVar(String name, float value) { variables[name] = value; }
float getVar(String name) { return variables.count(name) ? variables[name] : 0; }

// Simple arithmetic evaluator: + - * / (left-to-right)
float evalNumber(String expr) {
  expr.trim();
  int opPos = -1; char op = 0;
  for (int i = 1; i < expr.length() - 1; ++i) {
    char c = expr[i];
    if (c=='+'||c=='-'||c=='*'||c=='/') { op=c; opPos=i; break; }
  }
  if (opPos != -1) {
    String left  = expr.substring(0, opPos);  left.trim();
    String right = expr.substring(opPos + 1); right.trim();
    float l = evalNumber(left);
    float r = evalNumber(right);
    switch (op) { case '+': return l+r; case '-': return l-r; case '*': return l*r; case '/': return (r!=0)? l/r : 0; }
  }
  if (variables.count(expr)) return variables[expr];
  return expr.toFloat();
}

// Boolean condition evaluator: >, <, >=, <=, ==, !=
bool evalCond(String cond) {
  cond.trim();
  const char* ops[] = {">=","<=","==","!=",">","<"};
  for (int i=0;i<6;i++) {
    String op = ops[i];
    int pos = cond.indexOf(op);
    if (pos != -1) {
      String L = cond.substring(0,pos); L.trim();
      String R = cond.substring(pos+op.length()); R.trim();
      float l = evalNumber(L);
      float r = evalNumber(R);
      if (op==">=") return l>=r;
      if (op=="<=") return l<=r;
      if (op=="==") return l==r;
      if (op=="!=") return l!=r;
      if (op==">")  return l>r;
      if (op=="<")  return l<r;
    }
  }
  return evalNumber(cond) != 0;
}

// ===== Built-ins =====
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

float readSensor() {
  if (!hdcInitialized) {
    if (hdc.begin()) {
      hdcInitialized = true;
      Serial.println("HDC1000 initialized");
    } else {
      Serial.println("HDC1000 not found!");
      return -999; // error
    }
  }
  return hdc.readTemperature();
}

void setLedRGB(int r, int g, int b) {
  rgb_led_1.setPixelColor(0, rgb_led_1.Color(r, g, b));
  rgb_led_1.show();
  Serial.printf("LED RGB set to (%d,%d,%d)\n", r, g, b);
}

// ===== Interpreter =====
void executeLine(String line, int& pc) {
  line.trim();
  if (line.length()==0) return;

  if (line.startsWith("if(")) {
    int s=line.indexOf('(')+1, e=line.indexOf(')');
    String cond = line.substring(s,e);
    if (!evalCond(cond)) {
      while (++pc < (int)scriptLines.size()) {
        String next = scriptLines[pc]; next.trim();
        if (!next.startsWith("else if") && !next.startsWith("else")) break;
      }
    }
    return;
  } else if (line.startsWith("else if(")) {
    int s=line.indexOf('(')+1, e=line.indexOf(')');
    String cond = line.substring(s,e);
    if (!evalCond(cond)) {
      while (++pc < (int)scriptLines.size()) {
        String next = scriptLines[pc]; next.trim();
        if (!next.startsWith("else if") && !next.startsWith("else")) break;
      }
    }
    return;
  } else if (line.startsWith("else")) {
    return;
  } else if (line.startsWith("while(")) {
    int s=line.indexOf('(')+1, e=line.indexOf(')');
    String cond = line.substring(s,e);
    int loopStart = pc;
    while (evalCond(cond) && runningScript) {
      int innerPc = loopStart + 1;
      while (innerPc < (int)scriptLines.size() && scriptLines[innerPc].startsWith(" ")) {
        String body = scriptLines[innerPc]; body.trim();
        executeLine(body, innerPc);
        innerPc++;
        pumpControl(); if (!runningScript) break;
      }
      yield(); pumpControl(); if (!runningScript) break;
    }
    while (++pc < (int)scriptLines.size()) {
      String next = scriptLines[pc]; next.trim();
      if (!next.startsWith(" ")) break;
    }
    return;
  } else if (line.startsWith("for(")) {
    int p1=line.indexOf('(')+1; int p2=line.indexOf(';');
    int p3=line.indexOf(';', p2+1); int p4=line.indexOf(')');
    String init = line.substring(p1,p2); init.trim();
    String cond = line.substring(p2+1,p3); cond.trim();
    String step = line.substring(p3+1,p4); step.trim();
    executeLine(init, pc);
    int loopStart = pc;
    while (evalCond(cond) && runningScript) {
      int innerPc = loopStart + 1;
      while (innerPc < (int)scriptLines.size() && scriptLines[innerPc].startsWith(" ")) {
        String body = scriptLines[innerPc]; body.trim();
        executeLine(body, innerPc);
        innerPc++;
        pumpControl(); if (!runningScript) break;
      }
      executeLine(step, pc);
      yield(); pumpControl(); if (!runningScript) break;
    }
    while (++pc < (int)scriptLines.size()) {
      String next = scriptLines[pc]; next.trim();
      if (!next.startsWith(" ")) break;
    }
    return;
  }

  // Function call or assignment
  int paren = line.indexOf('(');
  int eq    = line.indexOf('=');

  if (paren != -1 && line.endsWith(")")) {
    String cmd  = line.substring(0, paren); cmd.trim();
    String args = line.substring(paren + 1, line.length()-1);
    if (commandMap.count(cmd)) { commandMap[cmd](args); return; }
  } else if (eq != -1) {
    String var = line.substring(0, eq); var.trim();
    String val = line.substring(eq + 1); val.trim();
    if (val == "readSensor") setVar(var, readSensor());
    else                      setVar(var, evalNumber(val));
    return;
  }

  Serial.println(String("Unknown command: ") + line);
}

void runScript() {
  do {
    int pc = 0;
    while (pc < (int)scriptLines.size() && runningScript) {
      pumpControl();
      String line = scriptLines[pc];
      executeLine(line, pc);
      pc++;
      yield();
    }
    pumpControl();
    yield();
  } while (runForever && runningScript);
}

// ===== NEW: flush helper that prints the completed message =====
inline void bleFlush(const char* reason) {
  String s = bleBuf; s.trim();
  if (s.length() == 0) return;
  Serial.printf("[BLE] FULL (%s): \"%s\"\n", reason, s.c_str());

  // Case-insensitive control words
  String up = s; up.toUpperCase();
  if      (up == "RUN")     processCompleteLine("RUN", "BLE");
  else if (up == "RUNLOOP") processCompleteLine("RUNLOOP", "BLE");
  else if (up == "STOP")    processCompleteLine("STOP", "BLE");
  else                      processCompleteLine(s, "BLE");

  bleBuf = "";
  bleParenDepth = 0;
  bleSawOpenParen = false;
}

// When a complete line arrives while idle (no parens), flush it.
inline void bleMaybeFlushByIdle() {
  if (bleBuf.length() == 0) return;
  if (bleParenDepth > 0)    return; // inside '(...' → wait for ')'
  uint32_t now = millis();
  if (now - lastBleByteMs >= BLE_IDLE_FLUSH_MS) {
    bleFlush("idle");
  }
}

// ===== Line enqueue / controls =====
void processCompleteLine(const String& line, const char* tag) {
  String s = line; s.trim();
  if (s.length() == 0) return;

  if (runningScript) {
    if (s == "STOP") {
      runForever = false; runningScript = false;
      scriptLines.clear(); variables.clear();
      Serial.printf("[%s] STOP received. Cleared script & variables.\n", tag);
    } else {
      Serial.printf("[%s] (ignored while running): %s\n", tag, s.c_str());
    }
    return;
  }

  if      (s == "RUN")     { runningScript = true; runForever = false; runScript(); runningScript = false; }
  else if (s == "RUNLOOP") { runningScript = true; runForever = true;  runScript(); /* keep running until STOP */ }
  else if (s == "STOP")    { runForever=false; runningScript=false; scriptLines.clear(); variables.clear(); Serial.printf("[%s] STOP (idle). Cleared.\n", tag); }
  else                     { scriptLines.push_back(s); Serial.printf("[%s] Queued: %s\n", tag, s.c_str()); }
}

// ===== Serial (USB) line assembly =====
void pumpUartSource(Stream& in, String& buf, const char* tag) {
  while (in.available()) {
    char c = in.read();
    if (c == '\n') {
      Serial.printf("[USB] FULL: \"%s\"\n", buf.c_str());
      processCompleteLine(buf, tag);
      buf = "";
    } else if (c != '\r') {
      buf += c;
    }
  }
}

String utf16leToString(uint8_t *data, size_t length)
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

// ===== BLE handler =====
void onBleConfigWrite() {
  uint8_t raw[64] = {0};
  SenseBoxBLE::read(raw, sizeof(raw));

  // Try to decode chunk (handles UTF-16LE & UTF-8)
  String chunk = utf16leToString(raw, sizeof(raw));
  Serial.printf("[BLE] chunk: \"%s\"\n", chunk.c_str());

  // Feed decoded text into assembler
  for (size_t i = 0; i < (size_t)chunk.length(); ++i) {
    char c = chunk[i];

    // ❗ Ignore CR/LF completely — some apps send them after every keystroke
    if (c == '\r' || c == '\n' || c == '\0') continue;

    // Accumulate
    bleBuf += c;

    // Track parentheses to detect completion of calls like led(...)
    if (c == '(') { bleParenDepth++; bleSawOpenParen = true; }
    else if (c == ')') { if (bleParenDepth > 0) bleParenDepth--; }

    // IMMEDIATE flush when a function call closes cleanly
    if (c == ')' && bleParenDepth == 0 && bleSawOpenParen) {
      bleFlush("paren");
    }
  }

  lastBleByteMs = millis(); // update “last seen” time for idle flush
}

// ===== Pump both inputs regularly =====
bool pumpControl() {
  pumpUartSource(Serial, usbBuf, "USB");
  SenseBoxBLE::poll();
  bleMaybeFlushByIdle();
  return runningScript;
}

// ===== Arduino setup/loop =====
void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 500) { delay(1); }

  setupCommandMap();

  rgb_led_1.begin();
  rgb_led_1.setBrightness(30);
  rgb_led_1.show();

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

  // BLE init
  SenseBoxBLE::start("senseBox-BLE");
  int rxHandle = SenseBoxBLE::setConfigCharacteristic(BLE_SERVICE_UUID, BLE_RX_UUID);
  if (rxHandle <= 0) Serial.println("BLE: setConfigCharacteristic failed or already set.");
  SenseBoxBLE::configHandler = onBleConfigWrite;
  SenseBoxBLE::setName("senseBox-BLE");
  SenseBoxBLE::advertise();

  Serial.println("BLE ready: service CF06...BC84, RX char 2CDF...B3A8");
}

void loop() {
  if (!runningScript) {
    if (Serial.available())  pumpUartSource(Serial, usbBuf, "USB");
    SenseBoxBLE::poll();
    bleMaybeFlushByIdle();
  } else {
    pumpControl();
  }
}
