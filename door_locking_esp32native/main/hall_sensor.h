#ifndef HALL_SENSOR_H
#define HALL_SENSOR_H

#include "esp_err.h"
#include "driver/gpio.h"

/**
 * @brief Initialize Hall sensor
 * @return ESP_OK on success, error code on failure
 */
esp_err_t hall_sensor_init(void);

/**
 * @brief Read Hall sensor state
 * @return true if magnet detected (LOW), false if no magnet (HIGH)
 */
bool hall_sensor_read(void);

/**
 * @brief Get last Hall sensor state
 * @return Last state (true = magnet detected, false = no magnet)
 */
bool hall_sensor_get_last_state(void);

/**
 * @brief Set Hall sensor callback for state changes
 * @param callback Function to call when state changes
 * @return ESP_OK on success, error code on failure
 */
esp_err_t hall_sensor_set_callback(void (*callback)(bool state));

/**
 * @brief Deinitialize Hall sensor
 * @return ESP_OK on success, error code on failure
 */
esp_err_t hall_sensor_deinit(void);

#endif // HALL_SENSOR_H
