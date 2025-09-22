#pragma once

class Sensor {
public:
    virtual ~Sensor() {}
    virtual float read() = 0;
private:
    virtual bool begin() = 0;
};