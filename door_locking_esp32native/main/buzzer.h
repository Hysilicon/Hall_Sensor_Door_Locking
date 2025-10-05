#ifndef BUZZER_H
#define BUZZER_H

#include "esp_err.h"
#include "driver/gpio.h"

/**
 * @brief Initialize buzzer
 * @return ESP_OK on success, error code on failure
 */
esp_err_t buzzer_init(void);

/**
 * @brief Start non-blocking beep sequence
 * @param times Number of beeps
 * @param duration Duration of each beep in milliseconds
 * @return ESP_OK on success, error code on failure
 */
esp_err_t buzzer_start_beep(int times, int duration);

/**
 * @brief Stop current beep sequence
 * @return ESP_OK on success, error code on failure
 */
esp_err_t buzzer_stop_beep(void);

/**
 * @brief Check if buzzer is currently active
 * @return true if active, false otherwise
 */
bool buzzer_is_active(void);

/**
 * @brief Update buzzer state (call in main loop)
 * This function handles the non-blocking beep state machine
 */
void buzzer_update(void);

/**
 * @brief Deinitialize buzzer
 * @return ESP_OK on success, error code on failure
 */
esp_err_t buzzer_deinit(void);

#endif // BUZZER_H
