#include "helpers/script_parser.h"

// ===== BlockType Identification =====

BlockType ScriptParser::identifyMarker(const String& line) {
    String trimmed = line;
    trimmed.trim();
    
    if (trimmed == "BEGIN_SETUP") return BlockType::BEGIN_SETUP;
    if (trimmed == "END_SETUP") return BlockType::END_SETUP;
    if (trimmed == "BEGIN_LOOP") return BlockType::BEGIN_LOOP;
    if (trimmed == "END_LOOP") return BlockType::END_LOOP;
    
    // Check for control commands (case-insensitive)
    String upper = trimmed;
    upper.toUpperCase();
    if (upper == "RUN" || upper == "LOOP" || upper == "STOP") {
        return BlockType::CONTROL_COMMAND;
    }
    
    return BlockType::NORMAL;
}

// ===== Marker-Only Check =====

bool ScriptParser::isMarkerOnly(const String& line) {
    String trimmed = line;
    trimmed.trim();
    return trimmed == "BEGIN_SETUP" || trimmed == "END_SETUP" ||
           trimmed == "BEGIN_LOOP" || trimmed == "END_LOOP";
}

// ===== Error Message Generation =====

String ScriptParser::getErrorMessage(ValidationError err, int lineNum) {
    String msg = "[PARSE ERROR] ";
    
    switch (err) {
        case ValidationError::MARKER_NOT_ON_OWN_LINE:
            msg += "Marker must appear on its own line";
            break;
        case ValidationError::DUPLICATE_SETUP:
            msg += "Duplicate SETUP_START detected";
            break;
        case ValidationError::DUPLICATE_LOOP:
            msg += "Duplicate LOOP_START detected";
            break;
        case ValidationError::SETUP_END_WITHOUT_START:
            msg += "SETUP_END without SETUP_START";
            break;
        case ValidationError::LOOP_START_BEFORE_SETUP_END:
            msg += "LOOP_START before SETUP_END";
            break;
        case ValidationError::UNCLOSED_SETUP:
            msg += "SETUP_START without SETUP_END";
            break;
        case ValidationError::UNCLOSED_LOOP:
            msg += "LOOP_START without LOOP_END";
            break;
        case ValidationError::ORPHANED_CODE_AFTER_LOOP:
            msg += "Code after LOOP_END is ignored";
            break;
        default:
            msg += "Unknown error";
            break;
    }
    
    if (lineNum >= 0) {
        msg += " at line ";
        msg += String(lineNum);
    }
    
    return msg;
}

// ===== Main Parser =====

ParseResult ScriptParser::parseBlocks(const std::vector<String>& allLines) {
    ScriptBlocks blocks;
    ParseResult result = {true, ValidationError::NONE, "", blocks};
    
    int setupStartIdx = -1;
    int setupEndIdx = -1;
    int loopStartIdx = -1;
    int loopEndIdx = -1;
    
    // First pass: find all markers
    for (int i = 0; i < (int)allLines.size(); i++) {
        BlockType type = identifyMarker(allLines[i]);
        
        if (type == BlockType::CONTROL_COMMAND) {
            // Control commands mark the end of script collection
            break;
        }
        
        if (type == BlockType::BEGIN_SETUP) {
            if (setupStartIdx != -1) {
                result.valid = false;
                result.error = ValidationError::DUPLICATE_SETUP;
                result.errorMsg = getErrorMessage(result.error, i);
                return result;
            }
            setupStartIdx = i;
        }
        else if (type == BlockType::END_SETUP) {
            if (setupStartIdx == -1) {
                result.valid = false;
                result.error = ValidationError::SETUP_END_WITHOUT_START;
                result.errorMsg = getErrorMessage(result.error, i);
                return result;
            }
            if (setupEndIdx != -1) {
                result.valid = false;
                result.error = ValidationError::DUPLICATE_SETUP;
                result.errorMsg = getErrorMessage(result.error, i);
                return result;
            }
            setupEndIdx = i;
        }
        else if (type == BlockType::BEGIN_LOOP) {
            if (setupEndIdx == -1 && setupStartIdx != -1) {
                result.valid = false;
                result.error = ValidationError::LOOP_START_BEFORE_SETUP_END;
                result.errorMsg = getErrorMessage(result.error, i);
                return result;
            }
            if (loopStartIdx != -1) {
                result.valid = false;
                result.error = ValidationError::DUPLICATE_LOOP;
                result.errorMsg = getErrorMessage(result.error, i);
                return result;
            }
            loopStartIdx = i;
        }
        else if (type == BlockType::END_LOOP) {
            if (loopStartIdx == -1) {
                result.valid = false;
                result.error = ValidationError::UNCLOSED_LOOP;
                result.errorMsg = getErrorMessage(result.error, i);
                return result;
            }
            if (loopEndIdx != -1) {
                result.valid = false;
                result.error = ValidationError::DUPLICATE_LOOP;
                result.errorMsg = getErrorMessage(result.error, i);
                return result;
            }
            loopEndIdx = i;
        }
    }
    
    // Check for unclosed blocks
    if (setupStartIdx != -1 && setupEndIdx == -1) {
        result.valid = false;
        result.error = ValidationError::UNCLOSED_SETUP;
        result.errorMsg = getErrorMessage(result.error, setupStartIdx);
        return result;
    }
    
    if (loopStartIdx != -1 && loopEndIdx == -1) {
        result.valid = false;
        result.error = ValidationError::UNCLOSED_LOOP;
        result.errorMsg = getErrorMessage(result.error, loopStartIdx);
        return result;
    }
    
    // Second pass: extract block contents
    // SETUP block
    if (setupStartIdx != -1 && setupEndIdx != -1) {
        for (int i = setupStartIdx + 1; i < setupEndIdx; i++) {
            result.blocks.setupLines.push_back(allLines[i]);
        }
        result.blocks.hasSetup = true;
    }
    
    // LOOP block
    if (loopStartIdx != -1 && loopEndIdx != -1) {
        for (int i = loopStartIdx + 1; i < loopEndIdx; i++) {
            result.blocks.loopLines.push_back(allLines[i]);
        }
        result.blocks.hasLoop = true;
    }
    
    // Backward compatibility: if no markers found, treat all as LOOP
    if (!result.blocks.hasSetup && !result.blocks.hasLoop) {
        for (const auto& line : allLines) {
            BlockType type = identifyMarker(line);
            if (type != BlockType::CONTROL_COMMAND && type != BlockType::NORMAL) {
                continue; // Skip any orphaned markers
            }
            if (type != BlockType::CONTROL_COMMAND) {
                result.blocks.loopLines.push_back(line);
            }
        }
        result.blocks.hasLoop = true;
    }
    
    // Log warnings
    if (loopEndIdx != -1 && loopEndIdx < (int)allLines.size() - 1) {
        Serial.printf("[PARSE] Warning: code after LOOP_END is ignored\n");
    }
    
    return result;
}

// ===== Validation =====

bool ScriptParser::validateStructure(const ScriptBlocks& blocks, String& errorMsg) {
    if (!blocks.hasLoop && !blocks.hasSetup) {
        errorMsg = "[VALIDATE] No code to execute";
        return false;
    }
    
    if (blocks.hasSetup && blocks.setupLines.size() == 0) {
        // Warning only
        Serial.println("[VALIDATE] Warning: SETUP block is empty");
    }
    
    if (blocks.hasLoop && blocks.loopLines.size() == 0) {
        errorMsg = "[VALIDATE] LOOP block is empty";
        return false;
    }
    
    errorMsg = "";
    return true;
}
