#pragma once
#include <Arduino.h>
#include <vector>

// Forward declaration
class CommandBuffer;

class BaseProtocol {
public:
    BaseProtocol() = default;

    virtual void setup() = 0;
    virtual bool begin() = 0;
    virtual void loop() = 0;
};

// Central command buffer and parser shared by all communication protocols
class CommandBuffer {
public:
    CommandBuffer();
    
    // Process a single character
    void processChar(char c);
    
    // Process a string (convenience method)
    void processString(const String& str);
    
    // Force flush current buffer
    void flush(const char* reason);
    
    // Check if idle timeout has passed and flush if needed
    void checkIdleFlush();
    
    // Reset buffer state
    void reset();
    
    // Get current buffer content (for debugging)
    String getBuffer() const { return buffer; }
    
private:
    String buffer;
    int parenDepth;
    int braceDepth;
    bool sawOpenParen;
    bool sawOpenBrace;
    uint32_t lastCharTime;
    bool justFlushed;
    
    static const uint32_t IDLE_FLUSH_MS = 800;
    
    // Helper to clean line from input artifacts
    String cleanLine(String line);
    
    // Check if line starts with a known command
    bool startsWithKnownCommand(const String& line);
    
    // Handle control commands (RUN, LOOP, STOP)
    void handleControlCommand(const String& cmd);
    
    // Add line to script
    void addScriptLine(const String& line);
};

extern CommandBuffer commandBuffer;