
#include "led.h"
#include "hw_config.h"
#include "hid_pedal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"

#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE            LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL         LEDC_CHANNEL_0
#define LEDC_DUTY_RES        LEDC_TIMER_8_BIT   /* 0–255 */
#define LEDC_FREQ_HZ         5000

#define DUTY_FAINT           50   /* dim when not connected */
#define DUTY_MAX             255  /* full brightness when connected */
#define POLL_MS              100

static void status_led_task(void *pvParameters)
{
    for (;;) {
        uint32_t duty = hid_pedal_is_connected() ? DUTY_MAX : DUTY_FAINT;
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

void led_start(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        return;
    }

    ledc_channel_config_t ch_cfg = {
        .gpio_num   = HW_GPIO_STATUS_LED,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
    };
    ret = ledc_channel_config(&ch_cfg);
    if (ret != ESP_OK) {
        return;
    }

    xTaskCreate(status_led_task, "status_led", 4096, NULL, 5, NULL);
}
