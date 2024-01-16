#include "wifi.hpp"

#include <Arduino.h>
#include <WiFi.h>

void Wifi::init(const char *ssid, const char *password) {
    _ssid = ssid;
    _password = password;
}

void Wifi::loop() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!_isStarted) {
            _isStarted = true;
            Serial.println("WiFi connected");
            Serial.println("IP address: ");
            Serial.println(WiFi.localIP());
        }
    } else {
        if (_isStarted) {
            _isStarted = false;
            Serial.println("WiFi disconnected");
        }
    }
}

void Wifi::start() { WiFi.begin(_ssid.c_str(), _password.c_str()); }

void Wifi::stop() { WiFi.disconnect(); }
