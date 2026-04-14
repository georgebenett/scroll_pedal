/*
 * Button driver: long-press and release events.
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Time (ms) button must be held to trigger long press (hold to power off). Used by power for wake-from-sleep too. */
#define BUTTON_LONG_PRESS_TIME_MS  2000

typedef enum {
    BUTTON_EVENT_PRESSED,
    BUTTON_EVENT_RELEASED,
    BUTTON_EVENT_LONG_PRESS,
} button_event_t;

typedef void (*button_callback_t)(button_event_t event, void *user_data);

/** Initialize button (uses HW_GPIO_BUTTON and timing from hw_config). */
esp_err_t button_init(void);

void button_register_callback(button_callback_t callback, void *user_data);
void button_start_monitoring(void);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */
