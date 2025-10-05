idf_component_register(SRCS "main.c"
                              "wifi_manager.c"
                              "mqtt_client.c"
                              "hall_sensor.c"
                              "buzzer.c"
                       PRIV_REQUIRES esp_wifi
                                      esp_netif
                                      esp_event
                                      esp_mqtt
                                      driver
                                      freertos
                                      esp_timer
                       INCLUDE_DIRS ".")
