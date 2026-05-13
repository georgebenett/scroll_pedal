/*
 * Button driver: long-press and release events, multi-instance.
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Time (ms) button must be held to fire BUTTON_EVENT_LONG_PRESS (triggers power off). */
#define BUTTON_LONG_PRESS_TIME_MS  15000

typedef enum {
    BUTTON_EVENT_PRESSED,
    BUTTON_EVENT_RELEASED,
    BUTTON_EVENT_LONG_PRESS,
} button_event_t;

typedef void (*button_callback_t)(button_event_t event, void *user_data);

typedef struct button_s *button_handle_t;

/**
 * Create a button instance on the given GPIO.
 * Must be called after gpio_install_isr_service().
 *
 * @param gpio           GPIO number
 * @param long_press_ms  Time held (ms) to fire BUTTON_EVENT_LONG_PRESS
 * @param active_low     True if pressed = GPIO low
 * @param out            Receives the created handle
 */
esp_err_t button_create(gpio_num_t gpio, uint32_t long_press_ms, bool active_low,
                        button_handle_t *out);

void button_register_callback(button_handle_t btn, button_callback_t callback, void *user_data);
void button_start_monitoring(button_handle_t btn);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */
