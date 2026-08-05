#include "communication/protocol.h"
#include "helpers/interpreter.h"
#include "helpers/script_parser.h"
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
  "sensor:", "buttonPressed(", "random(", "lightBoard=sensor:board:light", "sensor:board:light",
  "sendBLE(", "sendSerial(",
  "BEGIN_SETUP", "END_SETUP", "BEGIN_LOOP", "END_LOOP",  // Block markers
  NULL
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
    
    // Check if it's a marker (should not be cleaned further)
    String upper = line;
    upper.toUpperCase();
    if (upper == "BEGIN_SETUP" || upper == "END_SETUP" || 
        upper == "BEGIN_LOOP" || upper == "END_LOOP") {
        return upper;  // Return normalized marker
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

void CommandBuffer::startScriptExecution() {
    if (runningScript) {
        Serial.println("[CMD] Script already running, stopping current script first...");
        runningScript = false;
        runForever = false;
        delay(100);
        scriptLines.clear();
        setupLines.clear();
        loopLines.clear();
        variables.clear();
        setupExecuted = false;
    }
    
    if (scriptLines.size() == 0) {
        Serial.println("[CMD] Ignoring start - no script lines available");
        return;
    }
    
    Serial.println("========== PARSING SCRIPT ==========");
    Serial.printf("Raw script has %d lines:\n", scriptLines.size());
    for (int i = 0; i < scriptLines.size(); i++) {
        Serial.printf("  [%d]: %s\n", i, scriptLines[i].c_str());
    }
    
    // Parse the script into Setup/Loop blocks
    ParseResult parseResult = ScriptParser::parseBlocks(scriptLines);
    
    if (!parseResult.valid) {
        Serial.println("[CMD] Script parsing failed!");
        Serial.println(parseResult.errorMsg);
        scriptLines.clear();
        setupLines.clear();
        loopLines.clear();
        return;
    }
    
    // Validate the structure
    String validationError = "";
    if (!ScriptParser::validateStructure(parseResult.blocks, validationError)) {
        Serial.println("[CMD] Script validation failed!");
        Serial.println(validationError);
        scriptLines.clear();
        setupLines.clear();
        loopLines.clear();
        return;
    }
    
    // Store the parsed blocks
    setupLines = parseResult.blocks.setupLines;
    loopLines = parseResult.blocks.loopLines;
    hasSetup = parseResult.blocks.hasSetup;
    hasLoop = parseResult.blocks.hasLoop;
    setupExecuted = false;  // Reset flag for new execution
    
    Serial.println("========== SCRIPT PARSED SUCCESSFULLY ==========");
    Serial.printf("SETUP block: %d lines\n", setupLines.size());
    for (int i = 0; i < setupLines.size(); i++) {
        Serial.printf("  [SETUP %d]: %s\n", i, setupLines[i].c_str());
    }
    Serial.printf("LOOP block: %d lines\n", loopLines.size());
    for (int i = 0; i < loopLines.size(); i++) {
        Serial.printf("  [LOOP %d]: %s\n", i, loopLines[i].c_str());
    }
    Serial.println("===============================================");
    
    clearDisplay();
    resetDisplayTextY();
    setLedRGB(0, 0, 0);
    runningScript = true;
    runForever = true;  // Always run infinitely in loop mode
    
    Serial.println("Starting LOOP (infinite)...");
    runScript();
    runningScript = false;
}

void CommandBuffer::handleControlCommand(const String& cmd) {
    String up = cmd;
    up.toUpperCase();
    
    if (up == "STOP") {
        runForever = false;
        runningScript = false;
        scriptLines.clear();
        setupLines.clear();
        loopLines.clear();
        variables.clear();
        setupExecuted = false;
        Serial.println("Stopped");
    }
}

void CommandBuffer::addScriptLine(const String& line) {
    String cleaned = cleanLine(line);
    if (cleaned.length() == 0) return;
    
    // Check if this line starts with BEGIN_SETUP - new script upload
    if (cleaned == "BEGIN_SETUP" && runningScript) {
        Serial.println("[CMD] BEGIN_SETUP detected - stopping current script to accept new upload");
        runForever = false;
        runningScript = false;
        delay(100);
        scriptLines.clear();
        setupLines.clear();
        loopLines.clear();
        variables.clear();
        setupExecuted = false;
    }
    
    if (runningScript && cleaned != "BEGIN_SETUP") {
        Serial.printf("[CMD] Ignoring line while script is running: \"%s\"\n", line.c_str());
        return;
    }
    
    // Check if line contains embedded markers like "END_SETUPdes1=23"
    // Split on common block markers
    std::vector<String> parts;
    String current = "";
    const char* markers[] = {"BEGIN_SETUP", "END_SETUP", "BEGIN_LOOP", "END_LOOP", NULL};
    
    int idx = 0;
    while (idx < cleaned.length()) {
        bool foundMarker = false;
        
        // Check each marker at current position
        for (int m = 0; markers[m] != NULL; m++) {
            String marker = markers[m];
            if (cleaned.substring(idx).startsWith(marker)) {
                // Found a marker - save current content if any
                String content = current;
                content.trim();
                if (content.length() > 0) {
                    parts.push_back(content);
                    current = "";
                }
                // Add the marker
                parts.push_back(marker);
                idx += marker.length();
                foundMarker = true;
                break;
            }
        }
        
        if (!foundMarker) {
            current += cleaned[idx];
            idx++;
        }
    }
    
    // Add any remaining content
    String content = current;
    content.trim();
    if (content.length() > 0) {
        parts.push_back(content);
    }
    
    // Add all parts to scriptLines
    bool hasEndLoop = false;
    for (const auto& part : parts) {
        String p = part;
        p.trim();
        if (p.length() > 0) {
            scriptLines.push_back(p);
            Serial.printf("[CMD] Added line [%d]: \"%s\"\n", scriptLines.size()-1, p.c_str());
            
            // Check if this is END_LOOP marker
            if (p == "END_LOOP") {
                hasEndLoop = true;
            }
        }
    }
    
    // If we received END_LOOP, automatically start script execution
    // (but not if a script is already running - that's corruption from sender resending)
    if (hasEndLoop) {
        if (runningScript) {
            Serial.println("[CMD] END_LOOP received but script already running - ignoring (sender resend?)");
            // Clear the corrupted scriptLines
            scriptLines.clear();
        } else {
            Serial.println("[CMD] END_LOOP received - starting script execution automatically");
            startScriptExecution();
        }
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
    if (up == "STOP") {
        handleControlCommand(s);
        return;
    }
    
    // Simply add the line as-is (no complex re-parsing)
    // The character-by-character processing already handled the structure
    // Note: END_LOOP will trigger automatic execution in addScriptLine()
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
}

void CommandBuffer::processString(const String& str) {
    for (size_t i = 0; i < str.length(); i++) {
        processChar(str[i]);
    }
}