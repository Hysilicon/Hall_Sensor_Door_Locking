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
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Read current state
    bool current_state = gpio_get_level(HALL_PIN);
    
    // Update state if changed
    if (current_state != last_state) {
        last_state = current_state;
        
        // Call callback if registered
        if (state_change_callback) {
            state_change_callback(current_state);
        }
    }
    
    // Clear interrupt
    gpio_intr_disable(HALL_PIN);
    
    // Re-enable interrupt after a short delay to prevent bouncing
    static BaseType_t xTaskWoken = pdFALSE;
    xTaskWoken = xHigherPriorityTaskWoken;
    if (xTaskWoken) {
        portYIELD_FROM_ISR();
    }
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
        .intr_type = GPIO_INTR_ANYEDGE,  // Trigger on both rising and falling edges
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << HALL_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // Enable internal pull-up
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure Hall sensor GPIO: %s", esp_err_to_name(ret));
        vSemaphoreDelete(hall_mutex);
        return ret;
    }
    
    // Install GPIO interrupt service
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
        vSemaphoreDelete(hall_mutex);
        return ret;
    }
    
    // Add ISR handler
    ret = gpio_isr_handler_add(HALL_PIN, hall_sensor_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(ret));
        vSemaphoreDelete(hall_mutex);
        return ret;
    }
    
    // Read initial state
    last_state = gpio_get_level(HALL_PIN);
    
    ESP_LOGI(TAG, "Hall sensor initialized on GPIO%d, initial state: %s", 
             HALL_PIN, last_state ? "HIGH (no magnet)" : "LOW (magnet detected)");
    
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
