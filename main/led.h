
#ifndef LED_H
#define LED_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the status LED task (uses HW_GPIO_STATUS_LED from hw_config.h).
 *        Call after gatts_init(). Faint when not connected, max brightness when connected.
 */
void led_start(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_H */
