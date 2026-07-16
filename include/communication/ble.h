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
    // Send a measurement value back to the connected host via BLE notify
    bool sendValue(float value);
    // Send a value tagged with a numeric identifier so the host can map it.
    // Transmitted as an 8-byte packet: [identifier(float)][value(float)].
    bool sendValue(float identifier, float value);
    
private:
    bool bleAvailable = false;
    // Handle of the characteristic used to send values back to the host
    int sendCharacteristicHandle = 0;
    // Convert UTF-16LE encoded data to a String.
    static String utf16leToString(uint8_t *data, size_t length);
    // Handle incoming BLE configuration writes.
    static void onBleConfigWrite();
    // Check and handle connection state changes
    static void checkConnectionState();
};

extern BLEModule bleModule;