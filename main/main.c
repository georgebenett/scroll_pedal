#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "hid_pedal.h"
#include "button.h"
#include "power.h"
#include "led.h"

static const char *TAG = "MAIN";

/* Window in microseconds to detect a double press. */
#define DOUBLE_PRESS_WINDOW_US  (400 * 1000)

static int64_t s_last_press_us = 0;

static void scroll_button_cb(button_event_t event, void *arg)
{
    switch (event) {
    case BUTTON_EVENT_PRESSED: {
        int64_t now = esp_timer_get_time();
        if (now - s_last_press_us < DOUBLE_PRESS_WINDOW_US) {
            hid_pedal_scroll(1);   /* double press → Up Arrow (scroll up) */
        } else {
            hid_pedal_scroll(-1);  /* single press → Down Arrow (scroll down) */
        }
        s_last_press_us = now;
        break;
    }
    case BUTTON_EVENT_RELEASED:
        hid_pedal_scroll(0);   /* key release */
        break;
    case BUTTON_EVENT_LONG_PRESS:
        /* handled by power module (hold to power off) */
        break;
    }
}

void app_main(void)
{
    /* Assert power latch as early as possible. */
    power_init();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = hid_pedal_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HID init failed: %s", esp_err_to_name(ret));
        return;
    }

    /* Install GPIO ISR service (shared by button driver). */
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    ESP_ERROR_CHECK(button_init());
    power_register_button_callback();
    button_register_callback(scroll_button_cb, NULL);
    button_start_monitoring();

    led_start();
}
