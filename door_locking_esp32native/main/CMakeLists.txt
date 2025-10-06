idf_component_register(SRCS "main.c"
                              "wifi_manager.c"
                             
                              "hall_sensor.c"
                              "buzzer.c"
                       PRIV_REQUIRES esp_wifi
                                      esp_netif
                                      esp_event
                                      mqtt
                                      driver
                                      freertos
                                      esp_timer
                                      nvs_flash
                       INCLUDE_DIRS ".")
