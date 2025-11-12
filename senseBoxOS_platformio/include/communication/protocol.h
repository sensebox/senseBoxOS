#pragma once

class BaseProtocol {
public:
    BaseProtocol() = default;

    virtual void setup() = 0;
    virtual bool begin() = 0;
    virtual void loop() = 0;
};