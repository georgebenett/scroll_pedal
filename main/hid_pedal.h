/*
 * BLE HID scroll pedal — advertises as "scroll pedal", sends mouse wheel reports.
 */

#ifndef HID_PEDAL_H
#define HID_PEDAL_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize BLE stack, GAP advertising, and HID device profile. */
esp_err_t hid_pedal_init(void);

/**
 * Send a mouse wheel report.
 * @param delta  Scroll amount: negative = down, positive = up, 0 = release.
 *               Clamped to int8 range [-127, 127].
 */
void hid_pedal_scroll(int8_t delta);

/** Returns true while a host is connected. */
bool hid_pedal_is_connected(void);

/** Disconnect the current BLE host (no-op if not connected). */
void hid_pedal_disconnect(void);

#ifdef __cplusplus
}
#endif

#endif /* HID_PEDAL_H */
