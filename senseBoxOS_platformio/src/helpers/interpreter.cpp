#include "helpers/interpreter.h"
#include "helpers/block.h"
// #include "peripherals/sensors/hdc.h"   // for readSensor()
#include "logic/eval.h"
#include "logic/var.h"
#include "commands.h"
// TODO: including everything here seems suboptimal

std::map<String, float> variables;
std::vector<String> scriptLines;
bool runningScript = false;
bool runForever = false;   // RUNLOOP mode

void executeLine(String line, int& pc) {
  line.trim();
  if (line.length()==0) return;

  // ===== IF / ELSE IF / ELSE — brace-aware (with indentation fallback) =====
  if (line.startsWith("if(")) {
    int s=line.indexOf('(')+1, e=line.indexOf(')', s);
    String condIf = line.substring(s,e);
    bool ifTrue = evalCond(condIf);
    Block ifBlk = getFollowingBlock(pc);

    int cursor = ifBlk.after;
    struct Branch { enum Type { ELSEIF, ELSE_ } type; String cond; Block blk; };
    std::vector<Branch> branches;

    while (cursor < (int)scriptLines.size()) {
      String head = scriptLines[cursor];
      String t = head; t.trim();
      if (t.startsWith("else if(")) {
        int s2=t.indexOf('(')+1, e2=t.indexOf(')', s2);
        String cond = t.substring(s2,e2);
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
  if (line.startsWith("else if(") || line.startsWith("else")) {
    Block b = getFollowingBlock(pc);
    pc = b.after - 1;
    return;
  }

  // ===== WHILE (indentation fallback for body) =====
  if (line.startsWith("while(")) {
    int s=line.indexOf('(')+1, e=line.indexOf(')', s);
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
  }

  // ===== FOR — now brace-aware (with indentation fallback) =====
  if (line.startsWith("for(")) {
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

  if (paren != -1 && line.endsWith(")")) {
    String cmd  = line.substring(0, paren); cmd.trim();
    String args = line.substring(paren + 1, line.length()-1);
    if (commandMap.count(cmd)) { commandMap[cmd](args); return; }
  } else if (eq != -1) {
    String var = line.substring(0, eq); var.trim();
    String val = line.substring(eq + 1); val.trim();
    if (val == "readSensor") setVar(var, evalNumber(val));
    else                     setVar(var, evalNumber(val));
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
        Serial.println("(ignored while running)");
      }
      buf = "";
    } else if (c != '\r') {
      buf += c;
    }
  }
  return acted;
}
