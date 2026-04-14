idf_component_register(SRCS
                             "main.c"
                             "power.c"
                             "button.c"
                             "led.c"
                             "hid_pedal.c"
                             "esp_hid_gap.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_hid nvs_flash esp_driver_ledc)
