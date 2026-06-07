idf_component_register(SRCS "screen_swipe.c" "st7789.c" "main.c" "app_logic.c" "connections.c"
                    INCLUDE_DIRS "." "./include"
                    EMBED_TXTFILES "certs/google_root_ca.pem"
                    REQUIRES lvgl ui esp_driver_i2c esp_driver_spi esp_driver_gpio esp_driver_mcpwm network_provisioning nvs_flash esp_wifi esp_event esp_netif cjson esp_http_server esp_http_client)