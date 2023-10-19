#include "ble.h"

#include <stdio.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "freertos/event_groups.h"

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
    0x0D, 0xFF, 0x5D, 0x00, 0x03, 0x00, 0x01, 0x18, 0x08, 0x64, 0x2A, 0xA0, 0xC8, 0x84};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x64,  // 100ms
    .adv_int_max = 0xA0,  // 200ms
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

#define BLE_ADV_DATA_SET_BIT BIT0

static EventGroupHandle_t s_ble_event_group;

static const char *TAG = "BLE";

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            xEventGroupSetBits(s_ble_event_group, BLE_ADV_DATA_SET_BIT);
            break;
        default:
            break;
    }
}

void ble_send_adv(void) {
    ESP_LOGI(TAG, "sending BLE advertisement");

    esp_ble_gap_start_advertising(&adv_params);
}

void ble_init() {
    s_ble_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    esp_ble_gap_register_callback(esp_gap_cb);

    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data_raw(adv_data_on_off, sizeof(adv_data_on_off)));

    EventBits_t bits = xEventGroupWaitBits(s_ble_event_group, BLE_ADV_DATA_SET_BIT, pdTRUE, pdTRUE,
                                           portMAX_DELAY);

    if (bits & BLE_ADV_DATA_SET_BIT) {
        ESP_LOGI(TAG, "BLE advertisement data set");
    } else {
        ESP_LOGE(TAG, "BLE advertisement data not set");
    }
}
