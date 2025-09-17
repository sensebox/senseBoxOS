#include "logic/time.h"
#include "logic/eval.h"
#include "helpers/interpreter.h"

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