/*
 * Button driver: interrupt-based, long-press and release events, multi-instance.
 */

#include "button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>
#include <stdlib.h>

#define TAG                  "BUTTON"
#define DEBOUNCE_MS          20
#define LONG_PRESS_CHECK_MS  50   /* Poll interval while held, to detect long press */
#define IDLE_TIMEOUT_MS      500  /* Block time when idle (no button activity) */
#define MAX_CALLBACKS        4

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

struct button_s {
    button_config_t     cfg;
    callback_entry_t    callbacks[MAX_CALLBACKS];
    TaskHandle_t        task_handle;
    volatile bool       isr_triggered;
};

static bool read_pressed(struct button_s *b)
{
    int l = gpio_get_level(b->cfg.gpio_num);
    return b->cfg.active_low ? (l == 0) : (l != 0);
}

static void notify(struct button_s *b, button_event_t event)
{
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (b->callbacks[i].in_use && b->callbacks[i].callback) {
            b->callbacks[i].callback(event, b->callbacks[i].user_data);
        }
    }
}

static void IRAM_ATTR button_isr_handler(void *arg)
{
    struct button_s *b = (struct button_s *)arg;
    BaseType_t woken = pdFALSE;
    b->isr_triggered = true;
    if (b->task_handle != NULL) {
        vTaskNotifyGiveFromISR(b->task_handle, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

static void button_task(void *pvParameters)
{
    struct button_s *b = (struct button_s *)pvParameters;
    bool pressed = false;
    bool long_sent = false;
    TickType_t press_start = 0;

    /* If button held at boot, wait for release */
    if (read_pressed(b)) {
        while (read_pressed(b)) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    notify(b, BUTTON_EVENT_RELEASED);

    esp_err_t err = gpio_isr_handler_add(b->cfg.gpio_num, button_isr_handler, b);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Button interrupt on GPIO %d", b->cfg.gpio_num);

    while (1) {
        TickType_t wait_ms = pressed ? LONG_PRESS_CHECK_MS : IDLE_TIMEOUT_MS;
        uint32_t n = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));

        if (n > 0 || b->isr_triggered) {
            b->isr_triggered = false;
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
            ulTaskNotifyTake(pdTRUE, 0);
            b->isr_triggered = false;
        }

        bool now = read_pressed(b);

        if (now && !pressed) {
            pressed = true;
            long_sent = false;
            press_start = xTaskGetTickCount();
            notify(b, BUTTON_EVENT_PRESSED);
        } else if (now && pressed) {
            uint32_t ms = (uint32_t)((xTaskGetTickCount() - press_start) * portTICK_PERIOD_MS);
            if (!long_sent && ms >= b->cfg.long_press_time_ms) {
                long_sent = true;
                notify(b, BUTTON_EVENT_LONG_PRESS);
            }
        } else if (!now && pressed) {
            pressed = false;
            notify(b, BUTTON_EVENT_RELEASED);
        }
    }
}

esp_err_t button_create(gpio_num_t gpio, uint32_t long_press_ms, bool active_low,
                        button_handle_t *out)
{
    struct button_s *b = calloc(1, sizeof(struct button_s));
    if (!b) return ESP_ERR_NO_MEM;

    b->cfg.gpio_num = gpio;
    b->cfg.long_press_time_ms = long_press_ms;
    b->cfg.active_low = active_low;

    gpio_config_t io = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    esp_err_t ret = gpio_config(&io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed: %s", esp_err_to_name(ret));
        free(b);
        return ret;
    }

    ESP_LOGI(TAG, "Button GPIO %d, long press %lu ms", gpio, (unsigned long)long_press_ms);
    *out = b;
    return ESP_OK;
}

void button_register_callback(button_handle_t btn, button_callback_t callback, void *user_data)
{
    struct button_s *b = btn;
    for (int i = 0; i < MAX_CALLBACKS; i++) {
        if (!b->callbacks[i].in_use) {
            b->callbacks[i].callback = callback;
            b->callbacks[i].user_data = user_data;
            b->callbacks[i].in_use = true;
            return;
        }
    }
    ESP_LOGW(TAG, "No free callback slot");
}

void button_start_monitoring(button_handle_t btn)
{
    struct button_s *b = btn;
    xTaskCreate(button_task, "button", 4096, b, 5, &b->task_handle);
}
