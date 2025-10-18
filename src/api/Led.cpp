#include "Led.h"

Led::Led(uint8_t pin)
    : _pin(pin), _IsOn(false)
{
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
}

void Led::on()
{
    digitalWrite(_pin, HIGH);
    this->_IsOn = true;
}

void Led::off()
{
    digitalWrite(_pin, LOW);
    this->_IsOn = false;
}

bool Led::toggle()
{
    if (_IsOn)
    {
        off();
        this->_IsOn = false;
    }
    else
    {
        on();
        this->_IsOn = true;
    }
    return _IsOn;
}

bool Led::isOn()
{
    return this->_IsOn;
}