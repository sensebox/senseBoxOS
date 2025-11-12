#include <SenseBoxBLE.h>
#include "helpers/interpreter.h"

class BLE {
public:    
    BLE() = default;
    // setup BLE
    bool bleStart();
    // Initialize BLE services
    bool bleBegin();
    // needs to be called regularly to process BLE events
    void bleLoop();
    
private:
    // flush helper that prints the completed message
    static void bleFlush(const char* reason);
    // When a complete line arrives while idle (no parens), flush it.
    static void bleMaybeFlushByIdle();
    // Convert UTF-16LE encoded data to a String.
    static String utf16leToString(uint8_t *data, size_t length);
    // Handle incoming BLE configuration writes.
    static void onBleConfigWrite();
};