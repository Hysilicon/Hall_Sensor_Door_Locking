#include "buzzer.h"
#include "config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

static const char *TAG = "BUZZER";

// Buzzer state variables
static bool beep_active = false;
static int beep_times = 0;
static int beep_duration = 0;
static uint64_t beep_start_time = 0;
static bool buzzer_state = false;  // Current buzzer output state
static int beep_count = 0;  // Current beep count

static SemaphoreHandle_t buzzer_mutex = NULL;

esp_err_t buzzer_init(void)
{
    // Create mutex for thread safety
    buzzer_mutex = xSemaphoreCreateMutex();
    if (buzzer_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create buzzer mutex");
        return ESP_FAIL;
    }
    
    // Configure GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << BUZZER_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure buzzer GPIO: %s", esp_err_to_name(ret));
        vSemaphoreDelete(buzzer_mutex);
        return ret;
    }
    
    // Initialize buzzer to OFF state
    gpio_set_level(BUZZER_PIN, 0);
    buzzer_state = false;
    
    ESP_LOGI(TAG, "Buzzer initialized on GPIO%d", BUZZER_PIN);
    return ESP_OK;
}

esp_err_t buzzer_start_beep(int times, int duration)
{
    if (buzzer_mutex == NULL) {
        return ESP_FAIL;
    }
    
    xSemaphoreTake(buzzer_mutex, portMAX_DELAY);
    
    // Stop any current beep sequence
    beep_active = false;
    gpio_set_level(BUZZER_PIN, 0);
    buzzer_state = false;
    
    // Set new beep parameters
    beep_times = times;
    beep_duration = duration;
    beep_count = 0;
    beep_start_time = esp_timer_get_time() / 1000;  // Convert to milliseconds
    beep_active = true;
    
    // Start first beep
    gpio_set_level(BUZZER_PIN, 1);
    buzzer_state = true;
    
    xSemaphoreGive(buzzer_mutex);
    
    ESP_LOGI(TAG, "Started beep sequence: %d times, %dms each", times, duration);
    return ESP_OK;
}

esp_err_t buzzer_stop_beep(void)
{
    if (buzzer_mutex == NULL) {
        return ESP_FAIL;
    }
    
    xSemaphoreTake(buzzer_mutex, portMAX_DELAY);
    
    beep_active = false;
    gpio_set_level(BUZZER_PIN, 0);
    buzzer_state = false;
    beep_count = 0;
    
    xSemaphoreGive(buzzer_mutex);
    
    ESP_LOGI(TAG, "Buzzer stopped");
    return ESP_OK;
}

bool buzzer_is_active(void)
{
    if (buzzer_mutex == NULL) {
        return false;
    }
    
    xSemaphoreTake(buzzer_mutex, portMAX_DELAY);
    bool active = beep_active;
    xSemaphoreGive(buzzer_mutex);
    
    return active;
}

void buzzer_update(void)
{
    if (!beep_active || buzzer_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(buzzer_mutex, portMAX_DELAY);
    
    if (beep_active) {
        uint64_t current_time = esp_timer_get_time() / 1000;  // Convert to milliseconds
        uint64_t elapsed = current_time - beep_start_time;
        
        if (buzzer_state) {
            // Currently beeping - check if time to stop
            if (elapsed >= beep_duration) {
                // Stop current beep
                gpio_set_level(BUZZER_PIN, 0);
                buzzer_state = false;
                beep_count++;
                beep_start_time = current_time;
                
                // ESP_LOGI(TAG, "Beep %d/%d completed", beep_count, beep_times);
                
                if (beep_count >= beep_times) {
                    // All beeps completed
                    beep_active = false;
                    ESP_LOGI(TAG, "Beep sequence completed");
                }
            }
        } else {
            // Currently silent - check if time to start next beep
            if (beep_count < beep_times && elapsed >= beep_duration) {
                // Start next beep
                gpio_set_level(BUZZER_PIN, 1);
                buzzer_state = true;
                beep_start_time = current_time;
            }
        }
    }
    
    xSemaphoreGive(buzzer_mutex);
}

esp_err_t buzzer_deinit(void)
{
    // Stop any active beep
    buzzer_stop_beep();
    
    // Reset GPIO to default state
    gpio_reset_pin(BUZZER_PIN);
    
    // Clean up mutex
    if (buzzer_mutex) {
        vSemaphoreDelete(buzzer_mutex);
        buzzer_mutex = NULL;
    }
    
    ESP_LOGI(TAG, "Buzzer deinitialized");
    return ESP_OK;
}