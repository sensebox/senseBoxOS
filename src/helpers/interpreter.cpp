#include "helpers/interpreter.h"
#include "helpers/block.h"
#include "helpers/command_parser.h"
#include "helpers/sensor_registry.h"
#include "helpers/script_parser.h"
#include "logic/eval.h"
#include "logic/var.h"
#include "commands.h"
#include "communication/ble.h"
#include "communication/serial.h"
#include "peripherals/display.h"
// TODO: including everything here seems suboptimal

std::vector<String> scriptLines;        // Legacy: combined (used for block parsing)
std::vector<String> setupLines;         // New: setup block only
std::vector<String> loopLines;          // New: loop block only
bool hasSetup = false;                  // Flag if setup block exists
bool hasLoop = false;                   // Flag if loop block exists
bool setupExecuted = false;             // Flag if setup has been run
bool runningScript = false;
bool runForever = false;                // LOOP mode
unsigned int lineDelay = 200;           // Delay in ms between lines (default: 200ms)

// Helper function to check if a line starts with a keyword (with optional space before '(')
bool startsWithKeyword(const String& line, const String& keyword) {
  String trimmed = line;
  trimmed.trim();
  
  // Check for "keyword(" without space
  if (trimmed.startsWith(keyword + "(")) {
    return true;
  }
  
  // Check for "keyword (" with space
  if (trimmed.startsWith(keyword + " (")) {
    return true;
  }
  
  // Check for just the keyword (for "else")
  if (keyword == "else" && trimmed.startsWith("else")) {
    return true;
  }
  
  return false;
}

void executeLine(String line, int& pc) {
  line.trim();
  if (line.length()==0) return;

  // ===== IF / ELSE IF / ELSE — brace-aware (with indentation fallback) =====
  if (startsWithKeyword(line, "if")) {
    // Find matching closing parenthesis for if condition
    int s = line.indexOf('(') + 1;
    int parenCount = 1;
    int e = s;
    while (e < line.length() && parenCount > 0) {
      if (line[e] == '(') parenCount++;
      else if (line[e] == ')') parenCount--;
      if (parenCount > 0) e++;
    }
    String condIf = line.substring(s, e);
    bool ifTrue = evalCond(condIf);
    Block ifBlk = getFollowingBlock(pc);

    int cursor = ifBlk.after;
    struct Branch { enum Type { ELSEIF, ELSE_ } type; String cond; Block blk; };
    std::vector<Branch> branches;

    while (cursor < (int)scriptLines.size()) {
      String head = scriptLines[cursor];
      String t = head; t.trim();
      if (startsWithKeyword(t, "else if")) {
        // Find matching closing parenthesis for else if condition
        int s2 = t.indexOf('(') + 1;
        int parenCount = 1;
        int e2 = s2;
        while (e2 < t.length() && parenCount > 0) {
          if (t[e2] == '(') parenCount++;
          else if (t[e2] == ')') parenCount--;
          if (parenCount > 0) e2++;
        }
        String cond = t.substring(s2, e2);
        Block b = getFollowingBlock(cursor);
        branches.push_back({Branch::ELSEIF, cond, b});
        cursor = b.after;
      } else if (t.startsWith("else")) {
        Block b = getFollowingBlock(cursor);
        branches.push_back({Branch::ELSE_, "", b});
        cursor = b.after;
        break;
      } else break;
    }

    bool ran = false;
    if (ifTrue) { runBlock(ifBlk.bodyStart, ifBlk.bodyEnd); ran = true; }
    else {
      for (auto &br : branches) {
        if (br.type == Branch::ELSEIF && evalCond(br.cond)) {
          runBlock(br.blk.bodyStart, br.blk.bodyEnd); ran = true; break;
        }
      }
      if (!ran) {
        for (auto &br : branches) {
          if (br.type == Branch::ELSE_) { runBlock(br.blk.bodyStart, br.blk.bodyEnd); ran = true; break; }
        }
      }
    }

    pc = cursor - 1;   // skip whole chain
    return;
  }

  // Standalone 'else if' / 'else' (without its 'if'): skip their bodies
  if (startsWithKeyword(line, "else if") || startsWithKeyword(line, "else")) {
    Block b = getFollowingBlock(pc);
    pc = b.after - 1;
    return;
  }

  // ===== WHILE (indentation fallback for body) =====
  if (startsWithKeyword(line, "while")) {
    // Find matching closing parenthesis for while condition
    int s = line.indexOf('(') + 1;
    int parenCount = 1;
    int e = s;
    while (e < line.length() && parenCount > 0) {
      if (line[e] == '(') parenCount++;
      else if (line[e] == ')') parenCount--;
      if (parenCount > 0) e++;
    }
    String cond = line.substring(s, e);
    int loopStart = pc;
    while (evalCond(cond) && runningScript) {
      int innerPc = loopStart + 1;
      while (innerPc < (int)scriptLines.size() && scriptLines[innerPc].startsWith(" ")) {
        String body = scriptLines[innerPc]; body.trim();
        executeLine(body, innerPc);
        innerPc++;
        pumpControl(); 
        if (!runningScript) break;
      }
      yield(); 
      pumpControl(); 
      if (!runningScript) break;
    }
    while (++pc < (int)scriptLines.size()) {
      String next = scriptLines[pc]; next.trim();
      if (!next.startsWith(" ")) break;
    }
    return;
  }

  // ===== FOR — now brace-aware (with indentation fallback) =====
  if (startsWithKeyword(line, "for")) {
    int p1=line.indexOf('(')+1; int p2=line.indexOf(';');
    int p3=line.indexOf(';', p2+1); int p4=line.indexOf(')');
    String init = line.substring(p1,p2); init.trim();
    String cond = line.substring(p2+1,p3); cond.trim();
    String step = line.substring(p3+1,p4); step.trim();

    // Run init once (in the current scope)
    int tmp = pc;
    executeLine(init, tmp);

    // Find body block (supports "{...}" or indentation)
    Block body = getFollowingBlock(pc);

    // Loop
    while (evalCond(cond) && runningScript) {
      runBlock(body.bodyStart, body.bodyEnd);
      if (!runningScript) break;
      // step
      int tmp2 = pc;
      executeLine(step, tmp2);
      pumpControl();
      yield();
    }

    // Skip entire body regardless of how it was delimited
    pc = body.after - 1;
    return;
  }

  // ===== Function call or assignment =====
  int paren = line.indexOf('(');
  int eq    = line.indexOf('=');

  // Check for assignment first (var = expression)
  // An assignment has '=' before any '(', or has '=' but no '('
  if (eq != -1 && (paren == -1 || eq < paren)) {
    // It's an assignment
    String var = line.substring(0, eq); var.trim();
    String val = line.substring(eq + 1); val.trim();
    
    Serial.printf("[INTERP] Assignment: var='%s', val='%s'\n", var.c_str(), val.c_str());
    
    // Check if value is a string literal (quoted)
    if (val.startsWith("\"") && val.endsWith("\"")) {
      // It's a string - store as string
      String stringValue = val.substring(1, val.length() - 1);
      setVar(var, stringValue);
    } else if (isSensorCommand(val)) {
      SensorCommand cmd = parseSensorCommand(val);
      if (cmd.isValid) {
        float sensorValue = sensorRegistry.readSensor(cmd.sensorType, cmd.measurement);
        setVar(var, sensorValue);
      } else {
        setVar(var, cmd.errorCode);
      }
    } else {
      // Evaluate as number (includes functions like random(), etc.)
      setVar(var, evalNumber(val));
    }
    return;
  }
  // Check for function call (cmd(...))
  else if (paren != -1 && line.endsWith(")")) {
    String cmd  = line.substring(0, paren); cmd.trim();
    String args = line.substring(paren + 1, line.length()-1);
    if (commandMap.count(cmd)) { commandMap[cmd](args); return; }
  }

  // Unknown commands are ignored: skip this line or any following block
  ignoreLine(line, pc);
  return;
}

// Ignore a line and skip any following block (brace-aware or indented)
void ignoreLine(String line, int& pc) {
  line.trim();
  if (line.length() == 0) return;

  // If this line introduces a block, skip the entire block
  if (startsWithKeyword(line, "if") || startsWithKeyword(line, "else if") || startsWithKeyword(line, "else")) {
    Block b = getFollowingBlock(pc);
    pc = b.after - 1;
    return;
  }

  if (startsWithKeyword(line, "while") || startsWithKeyword(line, "for")) {
    Block b = getFollowingBlock(pc);
    pc = b.after - 1;
    return;
  }

  // Otherwise it's a single line — nothing more to do (caller will advance pc)
}

void runScript() {
  // Execute setup block once (if it exists)
  if (hasSetup && !setupExecuted) {
    Serial.println("[SCRIPT] Executing SETUP block...");
    runSetupBlock();
    setupExecuted = true;
  }
  
  // Execute loop block (once for RUN, repeatedly for LOOP)
  if (hasLoop) {
    do {
      Serial.println("[SCRIPT] Executing LOOP block...");
      runLoopBlock();
      pumpControl();
      yield();
    } while (runForever && runningScript);
  }
  
  Serial.println("[SCRIPT] Execution complete");
}

void runSetupBlock() {
  if (!runningScript) return;
  
  // Copy setup lines into scriptLines for block parsing
  scriptLines.clear();
  scriptLines = setupLines;
  
  resetDisplayTextY();
  int pc = 0;
  
  while (pc < (int)scriptLines.size() && runningScript) {
    pumpControl();
    String line = scriptLines[pc];
    executeLine(line, pc);
    pc++;
    
    // Non-blocking delay
    if (lineDelay > 0) {
      unsigned long startDelay = millis();
      while (millis() - startDelay < lineDelay && runningScript) {
        pumpControl();
        delay(1);
        yield();
      }
    }
    yield();
  }
}

void runLoopBlock() {
  if (!runningScript) return;
  
  // Copy loop lines into scriptLines for block parsing
  scriptLines.clear();
  scriptLines = loopLines;
  
  resetDisplayTextY();
  int pc = 0;
  
  while (pc < (int)scriptLines.size() && runningScript) {
    pumpControl();
    String line = scriptLines[pc];
    executeLine(line, pc);
    pc++;
    
    // Non-blocking delay
    if (lineDelay > 0) {
      unsigned long startDelay = millis();
      while (millis() - startDelay < lineDelay && runningScript) {
        pumpControl();
        delay(1);
        yield();
      }
    }
    yield();
  }
}

// Read control commands while script is running (so STOP works in LOOP)
void pumpControl() {
  bleModule.loop();
  serialModule.loop();
  // Ensure periodic sensor updates while a script is running (LOOP mode)
  sensorRegistry.pollSensors();
}
