#include "helpers/block.h"
#include "helpers/interpreter.h"

int countChar(const String& s, char ch) {
  int c = 0;
  for (int i = 0; i < s.length(); ++i) if (s[i] == ch) c++;
  return c;
}

// End index (exclusive) of an indented block that follows ctrlIdx
int indentedBlockEndAfter(int ctrlIdx) {
  int i = ctrlIdx + 1;
  while (i < (int)scriptLines.size() && scriptLines[i].startsWith(" ")) i++;
  return i; // first non-indented line
}

Block getFollowingBlock(int ctrlIdx) {
  Block b { ctrlIdx + 1, ctrlIdx + 1, ctrlIdx + 1, false };
  const int N = (int)scriptLines.size();

  // Case A: brace on same line e.g. "if(x) {", "for(...){"
  String head = scriptLines[ctrlIdx];
  if (head.indexOf('{') != -1) {
    int depth = countChar(head, '{') - countChar(head, '}');
    int j = ctrlIdx + 1;
    while (j < N && depth > 0) {
      depth += countChar(scriptLines[j], '{');
      depth -= countChar(scriptLines[j], '}');
      j++;
    }
    int closeIdx = j - 1;     // index containing the matching '}'
    b.bodyStart = ctrlIdx + 1;
    b.bodyEnd   = closeIdx;   // exclusive of '}'
    b.after     = closeIdx + 1;
    b.hasBraces = true;
    return b;
  }

  // Case B: brace on next non-empty line (line is "{" alone)
  int next = ctrlIdx + 1;
  while (next < N) {
    String t = scriptLines[next]; t.trim();
    if (t.length() == 0) { next++; continue; }
    break;
  }
  if (next < N) {
    String t = scriptLines[next]; t.trim();
    if (t == "{") {
      int depth = 1;
      int j = next + 1;
      while (j < N && depth > 0) {
        depth += countChar(scriptLines[j], '{');
        depth -= countChar(scriptLines[j], '}');
        j++;
      }
      int closeIdx = j - 1;
      b.bodyStart = next + 1;
      b.bodyEnd   = closeIdx; // exclusive of '}'
      b.after     = closeIdx + 1;
      b.hasBraces = true;
      return b;
    }
  }

  // Case C: indentation fallback (lines starting with a space)
  int end = indentedBlockEndAfter(ctrlIdx);
  b.bodyStart = ctrlIdx + 1;
  b.bodyEnd   = end;
  b.after     = end;
  b.hasBraces = false;
  return b;
}

void runBlock(int start, int end) {
  int lineIndex = start;
  while (lineIndex < end && runningScript) {
    String body = scriptLines[lineIndex]; body.trim();
    executeLine(body, lineIndex);
    lineIndex++;
    pumpControl();
    if (!runningScript) break;
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
