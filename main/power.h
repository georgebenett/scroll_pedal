/*
 * Power control: latch and hold-button-to-turn-off (same logic as gb_remote).
 */

#ifndef POWER_H
#define POWER_H

#include "esp_err.h"
#include "button.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize power: check wake-from-sleep, then assert power latch.
 *        Call as early as possible in app_main. May not return (re-enters deep sleep).
 */
void power_init(void);

/**
 * @brief Register the power button callback on the given button handle. Call after button_create().
 */
void power_register_button_callback(button_handle_t btn);

/**
 * @brief Turn off the device: release power latch and enter deep sleep. Button wake re-powers on.
 */
void power_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_H */
