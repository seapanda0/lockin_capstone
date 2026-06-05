idf_component_register(SRCS "st7789.c" "main.c" "app_logic.c"
                    INCLUDE_DIRS "." "./include"
                    REQUIRES lvgl ui esp_driver_i2c esp_driver_spi esp_driver_gpio esp_driver_mcpwm freertos)