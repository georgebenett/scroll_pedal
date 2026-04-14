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

/* ---------------------------------------------------------------------------
 * Battery & power
 * --------------------------------------------------------------------------- */
/** Battery voltage measurement (ADC) */
#define HW_GPIO_BATTERY_VOLTAGE      1
/** Battery charging status input */
#define HW_GPIO_BATTERY_CHARGE_STAT  2
/** Battery probe pin */
#define HW_GPIO_BATTERY_PROBE        3
/** Power enable — assert as early as possible in boot to latch power */
#define HW_GPIO_POWER_ENABLE         4


/* ---------------------------------------------------------------------------
 * User input
 * --------------------------------------------------------------------------- */
/** Button input */
#define HW_GPIO_BUTTON               11

#endif /* HW_CONFIG_H */
