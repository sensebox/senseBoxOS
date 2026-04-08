#pragma once

#include <Arduino.h>

struct Block {
  int  bodyStart;   // inclusive
  int  bodyEnd;     // exclusive
  int  after;       // first index after the block (or chain)
  bool hasBraces;
};

int countChar(const String& s, char ch);
int indentedBlockEndAfter(int ctrlIdx);
Block getFollowingBlock(int ctrlIdx);
void runBlock(int start, int end);
