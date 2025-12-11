#include "communication/protocol.h"
#include "helpers/interpreter.h"

class SerialModule: public BaseProtocol {
public:
    SerialModule() = default;
    void setup() override;
    bool begin() override;
    // needs to be called regularly to process Serial events
    void loop() override;
};

extern SerialModule serialModule;