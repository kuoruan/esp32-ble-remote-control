#include "led.hpp"

#include <Arduino.h>

void Led::init(int pin) { _pin = pin; }

void Led::loop() {}

void Led::on() {
    digitalWrite(_pin, HIGH);
    Serial.println("LED on");
    _isOn = true;
}

void Led::off() {
    digitalWrite(_pin, LOW);
    Serial.println("LED off");
    _isOn = false;
}

void Led::toggle() {
    if (_isOn) {
        off();
    } else {
        on();
    }
}
