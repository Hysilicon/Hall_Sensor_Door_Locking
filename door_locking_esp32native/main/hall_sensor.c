#include "hall_sensor.h"
#include "config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "HALL_SENSOR";

static bool last_state = true;  // HIGH = no magnet, LOW = magnet detected
static void (*state_change_callback)(bool state) = NULL;
static SemaphoreHandle_t hall_mutex = NULL;

/**
 * @brief GPIO interrupt handler for Hall sensor
 */
static void IRAM_ATTR hall_sensor_isr_handler(void* arg)
{
    // Simplified ISR - just update the state
    // The main task will handle the logic
    last_state = gpio_get_level(HALL_PIN);
}

esp_err_t hall_sensor_init(void)
{
    // Create mutex for thread safety
    hall_mutex = xSemaphoreCreateMutex();
    if (hall_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create Hall sensor mutex");
        return ESP_FAIL;
    }
    
    // Configure GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << HALL_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure Hall sensor GPIO: %s", esp_err_to_name(ret));
        vSemaphoreDelete(hall_mutex);
        return ret;
    }
    
    // Read initial state
    last_state = gpio_get_level(HALL_PIN);
    
    ESP_LOGI(TAG, "Hall sensor initialized on GPIO%d", HALL_PIN);
    
    return ESP_OK;
}

bool hall_sensor_read(void)
{
    if (hall_mutex == NULL) {
        return false;
    }
    
    xSemaphoreTake(hall_mutex, portMAX_DELAY);
    bool state = gpio_get_level(HALL_PIN);
    xSemaphoreGive(hall_mutex);
    
    return state;
}

bool hall_sensor_get_last_state(void)
{
    if (hall_mutex == NULL) {
        return false;
    }
    
    xSemaphoreTake(hall_mutex, portMAX_DELAY);
    bool state = last_state;
    xSemaphoreGive(hall_mutex);
    
    return state;
}

esp_err_t hall_sensor_set_callback(void (*callback)(bool state))
{
    if (hall_mutex == NULL) {
        return ESP_FAIL;
    }
    
    xSemaphoreTake(hall_mutex, portMAX_DELAY);
    state_change_callback = callback;
    xSemaphoreGive(hall_mutex);
    
    ESP_LOGI(TAG, "Hall sensor callback registered");
    return ESP_OK;
}

esp_err_t hall_sensor_re_enable_interrupt(void)
{
    if (hall_mutex == NULL) {
        return ESP_FAIL;
    }
    
    xSemaphoreTake(hall_mutex, portMAX_DELAY);
    
    // Re-enable interrupt after debounce
    gpio_intr_enable(HALL_PIN);
    
    xSemaphoreGive(hall_mutex);
    
    return ESP_OK;
}

esp_err_t hall_sensor_deinit(void)
{
    // Remove ISR handler
    if (gpio_isr_handler_remove(HALL_PIN) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to remove ISR handler");
    }
    
    // Uninstall GPIO interrupt service
    gpio_uninstall_isr_service();
    
    // Reset GPIO to default state
    gpio_reset_pin(HALL_PIN);
    
    // Clean up mutex
    if (hall_mutex) {
        vSemaphoreDelete(hall_mutex);
        hall_mutex = NULL;
    }
    
    // Clear callback
    state_change_callback = NULL;
    
    ESP_LOGI(TAG, "Hall sensor deinitialized");
    return ESP_OK;
}
