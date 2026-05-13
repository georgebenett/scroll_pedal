/*
 * Power control: latch power on boot, inactivity timeout to power off.
 */

#include "power.h"
#include "button.h"
#include "hw_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#define TAG "POWER"
#define WAKE_DEBOUNCE_MS   50

/* Power off after this many microseconds with no button activity. */
#define INACTIVITY_TIMEOUT_US  ((int64_t)10 * 60 * 1000 * 1000)  /* 10 minutes */

static esp_timer_handle_t s_inactivity_timer;

static void power_enter_sleep(void)
{
    ESP_LOGI(TAG, "Shutdown: enabling button wake, releasing power latch");
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << HW_GPIO_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);

    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable(HW_GPIO_BUTTON, GPIO_INTR_LOW_LEVEL);
    gpio_set_level(HW_GPIO_POWER_ENABLE, 0);
    esp_deep_sleep_start();
}

/** Returns false if we should go back to sleep (does not return). */
static bool power_check_wake_from_sleep(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_GPIO) {
        return true; /* Normal boot */
    }

    ESP_LOGI(TAG, "Woke from deep sleep via GPIO");
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << HW_GPIO_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    vTaskDelay(pdMS_TO_TICKS(WAKE_DEBOUNCE_MS));

    /* Active-low: pressed = 0 */
    bool pressed = (gpio_get_level(HW_GPIO_BUTTON) == 0);
    if (!pressed) {
        ESP_LOGI(TAG, "Button not held after wake — going back to sleep");
        power_enter_sleep();
    }

    ESP_LOGI(TAG, "Button held — powering on");
    return true;
}

static void inactivity_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "Inactivity timeout — shutting down");
    power_shutdown();
}

static void power_button_callback(button_event_t event, void *user_data)
{
    (void)user_data;
    switch (event) {
        case BUTTON_EVENT_PRESSED:
        case BUTTON_EVENT_RELEASED:
            /* Any activity resets the inactivity timer. */
            esp_timer_stop(s_inactivity_timer);
            esp_timer_start_once(s_inactivity_timer, INACTIVITY_TIMEOUT_US);
            break;
        case BUTTON_EVENT_LONG_PRESS:
            ESP_LOGI(TAG, "15 s hold — shutting down");
            power_shutdown();
            break;
    }
}

void power_init(void)
{
    if (!power_check_wake_from_sleep()) {
        power_enter_sleep();
    }

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << HW_GPIO_POWER_ENABLE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(HW_GPIO_POWER_ENABLE, 1);
    ESP_LOGI(TAG, "Power latch asserted");

    esp_timer_create_args_t timer_args = {
        .callback = inactivity_timer_cb,
        .name = "inactivity",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_inactivity_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(s_inactivity_timer, INACTIVITY_TIMEOUT_US));
    ESP_LOGI(TAG, "Inactivity timeout: 5 minutes");
}

void power_register_button_callback(button_handle_t btn)
{
    button_register_callback(btn, power_button_callback, NULL);
}

void power_shutdown(void)
{
    ESP_LOGI(TAG, "Preparing shutdown");
    vTaskDelay(pdMS_TO_TICKS(100));
    power_enter_sleep();
}
