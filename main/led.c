#include "led.h"

#include <stdio.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#define LED_ON 0
#define LED_OFF 1

static uint8_t s_led_state = LED_OFF;

#define LED_GPIO_PIN GPIO_NUM_10

static const char *TAG = "LED";

void led_on(void) {
    ESP_LOGI(TAG, "turning LED on");

    s_led_state = LED_ON;

    gpio_set_level(LED_GPIO_PIN, s_led_state);
}

void led_off(void) {
    ESP_LOGI(TAG, "turning LED off");

    s_led_state = LED_OFF;

    gpio_set_level(LED_GPIO_PIN, s_led_state);
}

void led_init() {
    ESP_ERROR_CHECK(gpio_reset_pin(LED_GPIO_PIN));
    ESP_ERROR_CHECK(gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT));

    led_off();
}
