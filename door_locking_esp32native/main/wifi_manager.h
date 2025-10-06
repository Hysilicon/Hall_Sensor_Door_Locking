#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi.h"

/**
 * @brief Initialize WiFi connection
 * @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_init(void);

/**
 * @brief Check if WiFi is connected
 * @return true if connected, false otherwise
 */
bool wifi_is_connected(void);

/**
 * @brief Get current WiFi IP address
 * @return IP address as string, NULL if not connected
 */
const char* wifi_get_ip_address(void);

/**
 * @brief Reconnect to WiFi if disconnected
 * @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_reconnect(void);

/**
 * @brief Deinitialize WiFi
 * @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_manager_deinit(void);

#endif // WIFI_MANAGER_H
