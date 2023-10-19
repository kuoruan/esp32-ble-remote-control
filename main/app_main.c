#include <inttypes.h>
#include <stdio.h>

#include "ble.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "led.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "wifi.h"

static const char *TAG = "MQTT";

#if CONFIG_APP_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t mqtt_certificate_pem_start[] =
    "-----BEGIN CERTIFICATE-----\n" CONFIG_APP_BROKER_CERTIFICATE_OVERRIDE
    "\n-----END CERTIFICATE-----";
#else
extern const uint8_t mqtt_certificate_pem_start[] asm("_binary_bemfa_com_pem_start");
#endif
extern const uint8_t mqtt_certificate_pem_end[] asm("_binary_bemfa_com_pem_end");

static const char *TOPIC = "projector006";

#define MQTT_DATA_ON "on"
#define MQTT_DATA_OFF "off"

static void turn_on(void) {
    led_on();
    ble_send_adv();
}

static void turn_off(void) {
    led_off();
    ble_send_adv();
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id,
                               void *event_data) {
    ESP_LOGD(TAG, "event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    int msg_id;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(event->client, TOPIC, 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA, topic=%.*s, data=%.*s", event->topic_len, event->topic,
                     event->data_len, event->data);

            // check event topic
            if (strncmp(event->topic, TOPIC, event->topic_len) != 0) {
                ESP_LOGI(TAG, "Received message on topic %.*s, but expected %s", event->topic_len,
                         event->topic, TOPIC);
                break;
            }

            if (strncmp(event->data, MQTT_DATA_ON, event->data_len) == 0) {
                turn_on();
            } else if (strncmp(event->data, MQTT_DATA_OFF, event->data_len) == 0) {
                turn_off();
            } else {
                ESP_LOGI(TAG, "Received unknown message: %.*s", event->data_len, event->data);
            }
            break;
        default:
            break;
    }
}

void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_APP_BROKER_URI,
        .broker.verification.certificate = (const char *)mqtt_certificate_pem_start,
        .credentials.client_id = CONFIG_APP_BROKER_CLIENT_ID,
        .credentials.username = CONFIG_APP_BROKER_USERNAME,
        .credentials.authentication.password = CONFIG_APP_BROKER_PASSWORD};

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_mqtt_client_start(client);
}

void app_main(void) {
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("wifi", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_ws", ESP_LOG_VERBOSE);

    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    led_init();
    ble_init();
    wifi_init();

    mqtt_app_start();
}
