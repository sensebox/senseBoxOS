#include "communication/protocol.h"
#include "helpers/interpreter.h"

class SerialModule: public BaseProtocol {
public:
    SerialModule() = default;
    void setup() override;
    bool begin() override;
    // needs to be called regularly to process Serial events
    void loop() override;
    // Send a measurement value back to the connected host via Serial
    void sendValue(const String& label, float value);
};

extern SerialModule serialModule;