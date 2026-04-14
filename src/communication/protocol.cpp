#include "communication/protocol.h"
#include "helpers/interpreter.h"
#include "peripherals/display.h"
#include "peripherals/led.h"

// This file provides the base implementation for communication protocols
// Individual implementations will inherit from this interface
// and provide their specific protocol logic

// The BaseProtocol class provides common functionality:
// - communication setup
// - commmunication begin
// - communication loop processing

// Global command buffer instance
CommandBuffer commandBuffer;

// List of known command prefixes
static const char* knownCommands[] = {
  "led(", "delay(", "display(", "clearDisplay(", "displayMeasurement(",
  "if(", "while(", "for(", "else", "}", 
  "sensor:", "buttonPressed(", "random(", "lightBoard=sensor:board:light", "sensor:board:light", NULL
};

// ===== CommandBuffer Implementation =====

CommandBuffer::CommandBuffer() 
    : parenDepth(0), braceDepth(0), sawOpenParen(false), 
      sawOpenBrace(false), lastCharTime(0), justFlushed(false) {
}

bool CommandBuffer::startsWithKnownCommand(const String& line) {
    for (int i = 0; knownCommands[i] != NULL; i++) {
        if (line.startsWith(knownCommands[i])) {
            return true;
        }
    }
    return false;
}

String CommandBuffer::cleanLine(String line) {
    line.trim();
    // Remove leading < or > followed by space (likely encoding artifacts from some BLE apps)
    while (line.length() > 1 && (line[0] == '<' || line[0] == '>') && line[1] == ' ') {
        line = line.substring(2);
        line.trim();
    }
    
    // Fix for garbage before commands: if line starts with 1-2 garbage chars 
    // followed by a known command, strip the garbage
    if (line.length() > 2 && !startsWithKnownCommand(line)) {
        // Try stripping 1 character
        String stripped1 = line.substring(1);
        if (startsWithKnownCommand(stripped1)) {
            Serial.printf("[CMD] Stripped garbage char '%c' from line\n", line[0]);
            return stripped1;
        }
        // Try stripping 2 characters
        if (line.length() > 3) {
            String stripped2 = line.substring(2);
            if (startsWithKnownCommand(stripped2)) {
                Serial.printf("[CMD] Stripped garbage chars '%s' from line\n", 
                             line.substring(0, 2).c_str());
                return stripped2;
            }
        }
    }
    
    return line;
}

void CommandBuffer::handleControlCommand(const String& cmd) {
    String up = cmd;
    up.toUpperCase();
    
    if (up == "RUN") {
        if (runningScript) {
            Serial.println("[CMD] Script already running, stopping current script first...");
            runningScript = false;
            runForever = false;
            delay(100);
            scriptLines.clear();
            variables.clear();
        }
        
        if (scriptLines.size() == 0) {
            Serial.println("[CMD] Ignoring RUN - no script lines available");
            return;
        }
        
        Serial.println("========== EXECUTING SCRIPT ==========");
        Serial.printf("Script has %d lines:\n", scriptLines.size());
        for (int i = 0; i < scriptLines.size(); i++) {
            Serial.printf("  [%d]: %s\n", i, scriptLines[i].c_str());
        }
        Serial.println("======================================");
        clearDisplay();
        resetDisplayTextY();
        setLedRGB(0, 0, 0);
        runningScript = true;
        runForever = false;
        runScript();
        runningScript = false;
    }
    else if (up == "LOOP") {
        if (runningScript) {
            Serial.println("[CMD] Script already running, stopping current script first...");
            runningScript = false;
            runForever = false;
            delay(100);
            scriptLines.clear();
            variables.clear();
        }
        
        if (scriptLines.size() == 0) {
            Serial.println("[CMD] Ignoring LOOP - no script lines available");
            return;
        }
        
        Serial.println("========== EXECUTING SCRIPT (LOOP) ==========");
        Serial.printf("Script has %d lines:\n", scriptLines.size());
        for (int i = 0; i < scriptLines.size(); i++) {
            Serial.printf("  [%d]: %s\n", i, scriptLines[i].c_str());
        }
        Serial.println("=============================================");
        clearDisplay();
        resetDisplayTextY();
        setLedRGB(0, 0, 0);
        runningScript = true;
        runForever = true;
        runScript();
    }
    else if (up == "STOP") {
        runForever = false;
        runningScript = false;
        scriptLines.clear();
        variables.clear();
        Serial.println("Stopped");
    }
}

void CommandBuffer::addScriptLine(const String& line) {
    if (runningScript) {
        Serial.printf("[CMD] Ignoring line while script is running: \"%s\"\n", line.c_str());
        return;
    }
    
    String cleaned = cleanLine(line);
    if (cleaned.length() > 0) {

        scriptLines.push_back(cleaned);
        Serial.printf("[CMD] Added line: \"%s\"\n", cleaned.c_str());
    }
}

void CommandBuffer::flush(const char* reason) {
    String s = buffer;
    s.trim();
    if (s.length() == 0) return;
    
    Serial.printf("[CMD] FLUSH (%s): \"%s\"\n", reason, s.c_str());
    
    // Clear buffer and state
    buffer = "";
    parenDepth = 0;
    braceDepth = 0;
    sawOpenParen = false;
    sawOpenBrace = false;
    
    // Check if it's a control command
    String up = s;
    up.toUpperCase();
    if (up == "RUN" || up == "LOOP" || up == "STOP") {
        handleControlCommand(s);
        return;
    }
    
    // Simply add the line as-is (no complex re-parsing)
    // The character-by-character processing already handled the structure
    addScriptLine(s);
}

void CommandBuffer::reset() {
    buffer = "";
    parenDepth = 0;
    braceDepth = 0;
    sawOpenParen = false;
    sawOpenBrace = false;
    justFlushed = false;
    lastCharTime = 0;
}

void CommandBuffer::checkIdleFlush() {
    if (buffer.length() == 0) return;
    if (parenDepth > 0) return;  // inside '(...' → wait for ')'
    if (braceDepth > 0) return;  // inside '{...' → wait for '}'
    
    uint32_t now = millis();
    if (now - lastCharTime >= IDLE_FLUSH_MS) {
        flush("idle");
    }
}

void CommandBuffer::processChar(char c) {
    lastCharTime = millis();
    
    // Handle newline (\n or \r) as explicit line break/flush
    if (c == '\n' || c == '\r') {
        Serial.printf("[CMD] Newline received, flushing buffer\n");
        flush("newline");
        return;
    }
    
    // Ignore other control characters (ASCII 0-31)
    if (c >= 0 && c < 32) {
        return;
    }
    
    // After a flush, ignore trailing whitespace and extra closing parens
    if (justFlushed && (c == ' ' || c == ')')) {
        return;
    }
    justFlushed = false;
    
    // Track parentheses and braces BEFORE adding character
    if (c == '(') { 
        parenDepth++; 
        sawOpenParen = true; 
    }
    else if (c == ')') { 
        if (parenDepth > 0) parenDepth--;
    }
    else if (c == '{') { 
        braceDepth++; 
        sawOpenBrace = true;
    }
    else if (c == '}') { 
        if (braceDepth > 0) braceDepth--;
    }
    
    // Accumulate character
    buffer += c;
    
    // Check if buffer starts with a control structure keyword
    bool isControlStructure = false;
    if (buffer.length() >= 2) {
        String bufTrimmed = buffer;
        bufTrimmed.trim();
        // Check for control structures with or without space: "if(" or "if ("
        if (bufTrimmed.startsWith("if(") || bufTrimmed.startsWith("if (") ||
            bufTrimmed.startsWith("while(") || bufTrimmed.startsWith("while (") ||
            bufTrimmed.startsWith("for(") || bufTrimmed.startsWith("for (") ||
            bufTrimmed.startsWith("else")) {
            isControlStructure = true;
        }
    }
    
    // Handle closing paren
    if (c == ')' && parenDepth == 0 && sawOpenParen) {
        // If inside braces and parens are now balanced, flush the command
        if (braceDepth > 0) {
            buffer.trim();
            addScriptLine(buffer);
            buffer = "";
            sawOpenParen = false;
            justFlushed = true;
            return;
        }
        
        // If NOT a control structure and no braces, flush immediately
        if (!isControlStructure && braceDepth == 0 && !sawOpenBrace) {
            flush("paren");
            justFlushed = true;
            return;
        }
        
        // For control structures, wait for the opening brace
    }
    
    // Handle opening brace
    if (c == '{') {
        // Flush the line with opening brace (e.g., "if (condition) {")
        buffer.trim();
        addScriptLine(buffer);
        buffer = "";
        justFlushed = true;
        return;
    }
    
    // Handle closing brace
    if (c == '}') {
        // Flush any content before the closing brace
        String beforeBrace = buffer.substring(0, buffer.length() - 1);
        beforeBrace.trim();
        if (beforeBrace.length() > 0) {
            addScriptLine(beforeBrace);
        }
        
        // Add closing brace as separate line
        addScriptLine("}");
        buffer = "";
        sawOpenBrace = false;
        justFlushed = true;
        return;
    }
    
    // Check if buffer ends with a control word (only if not inside parens/braces)
    if (parenDepth == 0 && braceDepth == 0 && buffer.length() >= 3) {
        String bufUpper = buffer;
        bufUpper.toUpperCase();
        
        String controlWord = "";
        int controlWordLen = 0;
        
        if (bufUpper.endsWith("RUN")) {
            controlWord = "RUN";
            controlWordLen = 3;
        } else if (bufUpper.endsWith("LOOP")) {
            controlWord = "LOOP";
            controlWordLen = 4;
        } else if (bufUpper.endsWith("STOP")) {
            controlWord = "STOP";
            controlWordLen = 4;
        }
        
        if (controlWordLen > 0) {
            Serial.printf("[CMD] Control word '%s' detected at end of buffer\n", 
                         controlWord.c_str());
            clearDisplay();
            resetDisplayTextY();
            // Extract everything before the control word
            String beforeControl = buffer.substring(0, buffer.length() - controlWordLen);
            beforeControl.trim();
            
            // Add what was before to script only if it's a valid command
            // (contains '=' for assignment or '(' for function call, or starts with known command)
            if (beforeControl.length() > 0) {
                if (beforeControl.indexOf('=') >= 0 || 
                    beforeControl.indexOf('(') >= 0 || 
                    startsWithKnownCommand(beforeControl)) {
                    addScriptLine(beforeControl);
                } else {
                    Serial.printf("[CMD] Ignoring invalid fragment before control word: \"%s\"\n", 
                                 beforeControl.c_str());
                }
            }
            
            // Now flush with just the control word
            buffer = controlWord;
            flush("control");
            justFlushed = true;
        }
    }
}

void CommandBuffer::processString(const String& str) {
    for (size_t i = 0; i < str.length(); i++) {
        processChar(str[i]);
    }
}