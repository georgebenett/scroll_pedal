/*
 * Hardware pinout and board configuration.
 * Central place for GPIO and other hardware assignments.
 */

#ifndef HW_CONFIG_H
#define HW_CONFIG_H

/* ---------------------------------------------------------------------------
 * System
 * --------------------------------------------------------------------------- */
/** Status LED (e.g. BLE advertising / connection state) */
#define HW_GPIO_STATUS_LED           42


/** Power enable — assert as early as possible in boot to latch power */
#define HW_GPIO_POWER_ENABLE         4


/* ---------------------------------------------------------------------------
 * User input
 * --------------------------------------------------------------------------- */
/** Single button: short press = scroll, hold 5 s = power off */
#define HW_GPIO_BUTTON               11

#endif /* HW_CONFIG_H */
