#pragma once
#include <Arduino.h>

class Led
{
public:
    explicit Led(uint8_t pin);
    void on();
    void off();
    bool toggle();
    bool isOn();

private:
    uint8_t _pin;
    bool _IsOn;
};