#pragma once

#include <Arduino.h>
#include <vector>

// ===== Data Structures =====

enum class BlockType {
    NORMAL,
    BEGIN_SETUP,
    END_SETUP,
    BEGIN_LOOP,
    END_LOOP,
    CONTROL_COMMAND  // RUN, LOOP, STOP
};

struct ScriptBlocks {
    std::vector<String> setupLines;
    std::vector<String> loopLines;
    bool hasSetup = false;
    bool hasLoop = false;
};

enum class ValidationError {
    NONE,
    MARKER_NOT_ON_OWN_LINE,      // Marker with trailing content
    DUPLICATE_SETUP,              // Multiple BEGIN_SETUP
    DUPLICATE_LOOP,               // Multiple BEGIN_LOOP
    SETUP_END_WITHOUT_START,      // END_SETUP before BEGIN_SETUP
    LOOP_START_BEFORE_SETUP_END,  // BEGIN_LOOP before END_SETUP
    UNCLOSED_SETUP,               // BEGIN_SETUP but no END
    UNCLOSED_LOOP,                // BEGIN_LOOP but no END
    ORPHANED_CODE_AFTER_LOOP      // Code after END_LOOP
};

struct ParseResult {
    bool valid;
    ValidationError error;
    String errorMsg;
    ScriptBlocks blocks;
};

// ===== Parser Class =====

class ScriptParser {
public:
    // Identify block marker type from a line
    static BlockType identifyMarker(const String& line);
    
    // Parse all script lines into Setup/Loop blocks
    static ParseResult parseBlocks(const std::vector<String>& allLines);
    
    // Validate the block structure
    static bool validateStructure(const ScriptBlocks& blocks, String& errorMsg);
    
private:
    // Helper to check if line contains only a marker (trimmed)
    static bool isMarkerOnly(const String& line);
    
    // Helper to get error message for validation error
    static String getErrorMessage(ValidationError err, int lineNum = -1);
};
