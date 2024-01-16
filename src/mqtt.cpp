#include "mqtt.hpp"

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

WiFiClient wifiClient;

PubSubClient mqttClient(wifiClient);

void Mqtt::init(const char *host, int port, const char *clientId, const char *username,
                const char *password) {
    _host = host;
    _port = port;
    _clientId = clientId;
    _username = username;
    _password = password;

    mqttClient.setServer(_host.c_str(), _port);
    mqttClient.setCallback([this](char *topic, byte *payload, unsigned int length) {
        if (_onMessageCallback) {
            _onMessageCallback(topic, (const char *)payload);
        }
    });
}

void Mqtt::loop() {
    if (!_isStarted) {
        return;
    }

    if (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect(_clientId.c_str(), _username.c_str(), _password.c_str())) {
            Serial.println("connected");
            mqttClient.subscribe("led");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }

    mqttClient.loop();
}

void Mqtt::start() {
    mqttClient.connect(_clientId.c_str(), _username.c_str(), _password.c_str());
    _isStarted = true;
}

void Mqtt::stop() {
    mqttClient.disconnect();
    _isStarted = false;
}

void Mqtt::publish(const char *topic, const char *payload) { mqttClient.publish(topic, payload); }

void Mqtt::subscribe(const char *topic) { mqttClient.subscribe(topic); }

void Mqtt::unsubscribe(const char *topic) { mqttClient.unsubscribe(topic); }

void Mqtt::onMessage(std::function<void(const char *topic, const char *payload)> callback) {
    _onMessageCallback = callback;
}
