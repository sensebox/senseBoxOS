#include "communication/protocol.h"
#include <SenseBoxBLE.h>
#include "helpers/interpreter.h"

class BLEModule: public BaseProtocol {
public:
    BLEModule() = default;
    // setup BLE
    void setup() override;
    // Initialize BLE services
    bool begin() override;
    // Initialize BLE services with custom device ID
    bool begin(String deviceId);
    // needs to be called regularly to process BLE events
    void loop() override;
    // Check if BLE is available
    bool isAvailable() const { return bleAvailable; }
    
private:
    bool bleAvailable = false;
    // Convert UTF-16LE encoded data to a String.
    static String utf16leToString(uint8_t *data, size_t length);
    // Handle incoming BLE configuration writes.
    static void onBleConfigWrite();
    // Check and handle connection state changes
    static void checkConnectionState();
};

extern BLEModule bleModule;