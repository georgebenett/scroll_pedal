/*
 * Button driver: interrupt-based, long-press and release events.
 */

#include "button.h"
#include "hw_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>

#define TAG           "BUTTON"
#define DEBOUNCE_MS   20
#define LONG_PRESS_CHECK_MS  50   /* Poll interval while held, to detect long press */
#define IDLE_TIMEOUT_MS      500  /* Block time when idle (no button activity) */
#define MAX_CALLBACKS 4

typedef struct {
    gpio_num_t gpio_num;
    uint32_t long_press_time_ms;
    bool active_low;
} button_config_t;

typedef struct {
    button_callback_t callback;
    void *user_data;
    bool in_use;
} callback_entry_t;

static button_config_t s_cfg;
static callback_entry_t s_callbacks[MAX_CALLBACKS];
static TaskHandle_t s_task_handle;
static volatile bool s_isr_triggered;

static bool read_pressed(void)
{
    int l = gpio_get_level(s_cfg.gpio_num);
    return s_cfg.active_low ? (l == 0) : (l != 0);
}

static void notify(button_event_t event)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (s_callbacks[i].in_use && s_callbacks[i].callback) {
            s_callbacks[i].callback(event, s_callbacks[i].user_data);
        }
    }
}

static void IRAM_ATTR button_isr_handler(void *arg)
{
    (void)arg;
    BaseType_t woken = pdFALSE;
    s_isr_triggered = true;
    if (s_task_handle != NULL) {
        vTaskNotifyGiveFromISR(s_task_handle, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

static void button_task(void *pvParameters)
{
    bool pressed = false;
    bool long_sent = false;
    TickType_t press_start = 0;

    /* If button held at boot, wait for release */
    if (read_pressed()) {
        while (read_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    notify(BUTTON_EVENT_RELEASED);

    esp_err_t err = gpio_isr_handler_add(s_cfg.gpio_num, button_isr_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Button interrupt on GPIO %d", s_cfg.gpio_num);

    while (1) {
        TickType_t wait_ms = pressed ? LONG_PRESS_CHECK_MS : IDLE_TIMEOUT_MS;
        uint32_t n = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));

        if (n > 0 || s_isr_triggered) {
            s_isr_triggered = false;
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
            ulTaskNotifyTake(pdTRUE, 0);
            s_isr_triggered = false;
        }

        bool now = read_pressed();

        if (now && !pressed) {
            pressed = true;
            long_sent = false;
            press_start = xTaskGetTickCount();
            notify(BUTTON_EVENT_PRESSED);
        } else if (now && pressed) {
            uint32_t ms = (uint32_t)((xTaskGetTickCount() - press_start) * portTICK_PERIOD_MS);
            if (!long_sent && ms >= s_cfg.long_press_time_ms) {
                long_sent = true;
                notify(BUTTON_EVENT_LONG_PRESS);
            }
        } else if (!now && pressed) {
            pressed = false;
            notify(BUTTON_EVENT_RELEASED);
        }
    }
}

esp_err_t button_init(void)
{
    s_cfg.gpio_num = HW_GPIO_BUTTON;
    s_cfg.long_press_time_ms = BUTTON_LONG_PRESS_TIME_MS;
    s_cfg.active_low = true;
    s_isr_triggered = false;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << s_cfg.gpio_num),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = s_cfg.active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    esp_err_t ret = gpio_config(&io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    memset(s_callbacks, 0, sizeof(s_callbacks));
    ESP_LOGI(TAG, "Button GPIO %d, long press %lu ms (interrupt)", s_cfg.gpio_num, (unsigned long)s_cfg.long_press_time_ms);
    return ESP_OK;
}

void button_register_callback(button_callback_t callback, void *user_data)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!s_callbacks[i].in_use) {
            s_callbacks[i].callback = callback;
            s_callbacks[i].user_data = user_data;
            s_callbacks[i].in_use = true;
            return;
        }
    }
    ESP_LOGW(TAG, "No free callback slot");
}

void button_start_monitoring(void)
{
    xTaskCreate(button_task, "button", 4096, NULL, 5, &s_task_handle);
}
