#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "esp_err.h"
#include "mqtt_client.h"

/**
 * @brief Initialize MQTT client
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_init(void);

/**
 * @brief Check if MQTT is connected
 * @return true if connected, false otherwise
 */
bool mqtt_is_connected(void);

/**
 * @brief Publish message to MQTT topic
 * @param topic MQTT topic
 * @param data Message data
 * @param len Message length
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_publish(const char* topic, const char* data, int len);

/**
 * @brief Subscribe to MQTT topic
 * @param topic MQTT topic to subscribe
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_subscribe(const char* topic);

/**
 * @brief Reconnect to MQTT broker
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_reconnect(void);

/**
 * @brief Deinitialize MQTT client
 * @return ESP_OK on success, error code on failure
 */
esp_err_t mqtt_deinit(void);

#endif // MQTT_CLIENT_H
