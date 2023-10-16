#include <esp_bt_main.h>
#include <inttypes.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_bt.h"
#include "esp_event.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

static const char *TAG = "MQTT";

#if CONFIG_APP_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t mqtt_certificate_pem_start[] =
    "-----BEGIN CERTIFICATE-----\n" CONFIG_APP_BROKER_CERTIFICATE_OVERRIDE
    "\n-----END CERTIFICATE-----";
#else
extern const uint8_t mqtt_certificate_pem_start[] asm("_binary_bemfa_com_pem_start");
#endif
extern const uint8_t mqtt_certificate_pem_end[] asm("_binary_bemfa_com_pem_end");

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_MAXIMUM_RETRY CONFIG_APP_WIFI_MAXIMUM_RETRY

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;

#define LED_GPIO_PIN GPIO_NUM_10

#define LED_ON 0
#define LED_OFF 1

static uint8_t s_led_state = LED_OFF;

static const char *TOPIC = "projector006";

#define MQTT_DATA_ON "on"
#define MQTT_DATA_OFF "off"

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/**
 * LEN | TYPE | VALUE
 * 2   | 0x01 | 0x04
 * 3   | 0x03 | 0x1218
 * 3   | 0x19 | 0x8001
 * 13  | 0xFF | 0x5D000300011808642AA0C884
 */
static uint8_t adv_data_on_off[] = {
    // Flags
    0x02, 0x01, 0x04,
    // Complete list of 16-bit Service UUIDs
    0x03, 0x03, 0x12, 0x18,
    // Appearance
    0x03, 0x19, 0x80, 0x01,
    // Manufacturer Specific Data
    0x0D, 0xFF, 0x5D, 0x00, 0x03, 0x00, 0x01,
    0x18, 0x08, 0x64, 0x2A, 0xA0, 0xC8, 0x84
};

static void turn_led_on(void) {
    ESP_LOGI(TAG, "turning LED on");

    s_led_state = LED_ON;

    gpio_set_level(LED_GPIO_PIN, s_led_state);
}

static void turn_led_off(void) {
    ESP_LOGI(TAG, "turning LED off");

    s_led_state = LED_OFF;

    gpio_set_level(LED_GPIO_PIN, s_led_state);
}

static void send_ble_adv(void) {
    ESP_LOGI(TAG, "sending BLE advertisement");

    esp_ble_gap_start_advertising(&adv_params);
}

static void turn_on(void) {
    turn_led_on();
    send_ble_adv();
}

static void turn_off(void) {
    turn_led_off();
    send_ble_adv();
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

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data) {
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if (s_retry_num < WIFI_MAXIMUM_RETRY) {
                ESP_LOGI(TAG, "retry to connect to the AP");

                esp_wifi_connect();
                s_retry_num++;
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            break;
        default:
            break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                             void *event_data) {
    switch (event_id) {
        case IP_EVENT_STA_GOT_IP:
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));

            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

            mqtt_app_start();
            break;
        default:
            break;
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = CONFIG_APP_WIFI_SSID,
                .password = CONFIG_APP_WIFI_PASSWORD,
            },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT)
     * or connection failed for the maximum number of re-tries (WIFI_FAIL_BIT).
     * The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we
     * can test which event actually happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID: %s", CONFIG_APP_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "failed to connect to SSID: %s, password: %s", CONFIG_APP_WIFI_SSID,
                 CONFIG_APP_WIFI_PASSWORD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void led_init() {
    ESP_ERROR_CHECK(gpio_reset_pin(LED_GPIO_PIN));
    ESP_ERROR_CHECK(gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT));

    turn_led_off();
}

void ble_init_gap() {
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data_raw(adv_data_on_off, sizeof(adv_data_on_off)));
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

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    led_init();
    ble_init_gap();

    wifi_init_sta();
}
