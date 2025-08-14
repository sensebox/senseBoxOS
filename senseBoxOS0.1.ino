// ESP32-S2 Pseudocode Interpreter with Multiline, Control Flow, delay(), OLED output, and stoppable RUNLOOP
// - Send lines over Serial, then send RUN (once) or RUNLOOP (repeat) or STOP (to halt)
// - Commands: display(expr), led(on|off), delay(ms), assignments, if/else if/else, while(...), for(init; cond; step)

/** Example script

temperature = readSensor
if(temperature > 25)
  led(on)
else
  led(off)
display(temperature)
delay(1000)
RUNLOOP
*/

#include <Arduino.h>
#include <map>
#include <vector>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>       // http://librarymanager/All#Adafruit_GFX_Library
#include <Adafruit_SSD1306.h>   // http://librarymanager/All#Adafruit_SSD1306
#include <Adafruit_NeoPixel.h>
#include <Adafruit_HDC1000.h>   // http://librarymanager/All#Adafruit_HDC1000_Library

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledInitialized = false;

// ===== NeoPixel & Sensor =====
Adafruit_NeoPixel rgb_led_1 = Adafruit_NeoPixel(1, 1, NEO_GRB + NEO_KHZ800);
Adafruit_HDC1000 hdc;
bool hdcInitialized = false;   // lazy init when first used

// ===== State =====
std::map<String, float> variables;
std::vector<String> scriptLines;
bool runningScript = false;
bool runForever   = false;   // RUNLOOP mode

// ===== Prototypes =====
float evalNumber(String expr);
bool  evalCond(String cond);
void  setVar(String name, float value);
float getVar(String name);
void  displayNumber(float value);
float readSensor();
void setLedRGB(int r, int g, int b);
void  executeLine(String line, int& pc);
void  runScript();
bool  pumpControl();   // read Serial while running to allow STOP

// ===== Command Dispatch =====
typedef void (*CommandHandler)(String args);
std::map<String, CommandHandler> commandMap;

void handleDisplay(String args) { displayNumber(evalNumber(args)); }

void handleLed(String args) {
  args.trim();
  if (args.indexOf(',') != -1) {
    int c1 = args.indexOf(',');
    int c2 = args.indexOf(',', c1 + 1);
    int r = evalNumber(args.substring(0, c1));
    int g = evalNumber(args.substring(c1 + 1, c2));
    int b = evalNumber(args.substring(c2 + 1));
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
  // interruptible delay: still process STOP while waiting
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

// Simple arithmetic evaluator: + - * / with left-to-right (no precedence nesting)
float evalNumber(String expr) {
  expr.trim();
  // scan for first operator (+,-,*,/) not at edges
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

// Boolean condition evaluator supporting >, <, >=, <=, ==, != with arithmetic on each side
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
      if (op==">=") return l>=r; if (op=="<=") return l<=r; if (op=="==") return l==r;
      if (op!="!=") {}
      if (op=="!=") return l!=r; if (op==">") return l>r; if (op=="<") return l<r;
    }
  }
  // no explicit comparator -> nonzero is true
  return evalNumber(cond) != 0;
}

// ===== Built-ins =====
void displayNumber(float value) {
  // Serial output
  Serial.print("Display: ");
  Serial.println(value);

  // OLED output (lazy init on first use)
  if (!oledInitialized) {
    if (oled.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { // address from your example
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

  // Print number on OLED (clears each time for simplicity)
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
    return; // unconditional block
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
      pumpControl();                    // allow STOP during RUNLOOP
      String line = scriptLines[pc];
      executeLine(line, pc);
      pc++;
      yield();
    }
    pumpControl();
    yield();
  } while (runForever && runningScript);
}

// Read control commands while script is running (so STOP works in RUNLOOP)
bool pumpControl() {
  static String buf;
  bool acted = false;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      buf.trim();
      if (buf == "STOP") {
        runForever    = false;
        runningScript = false;
        scriptLines.clear();
        variables.clear();
        Serial.println("Stopping script (STOP received). Cleared script and variables.");
        acted = true;
      } else if (buf == "RUN" || buf == "RUNLOOP") {
        // ignore during execution
      } else if (buf.length() > 0) {
        // Ignore arbitrary lines while running to avoid confusion
        Serial.println("(ignored while running)");
      }
      buf = "";
    } else if (c != '\r') {
      buf += c;
    }
  }
  return acted;
}

// ===== Arduino setup/loop =====
void setup() {
  Serial.begin(115200);
  while (!Serial);
  setupCommandMap();
  rgb_led_1.begin();
  rgb_led_1.setBrightness(30);
  rgb_led_1.show();

  // OLED init at boot (optional); we also lazy-init in displayNumber()
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

  Serial.println("Ready. Send lines, then RUN or RUNLOOP. Use STOP to halt.");
}

void loop() {
  if (Serial.available()) {
    static String line;
    char c = Serial.read();
    if (c == '\n') {
      line.trim();
      Serial.print("Got line: "); Serial.println(line);
      if (line == "RUN") {
        runningScript = true; runForever = false; runScript();
        runningScript = false;
      } else if (line == "RUNLOOP") {
        runningScript = true; runForever = true;  runScript();
        // remains running until STOP
      } else if (line == "STOP") {
        runForever = false; runningScript = false;
        scriptLines.clear();
        variables.clear();
        Serial.println("Stopped and cleared script & variables.");
      } else if (line.length() > 0) {
        scriptLines.push_back(line);
      }
      line = "";
    } else if (c != '\r') {
      line += c;
    }
  }
}
