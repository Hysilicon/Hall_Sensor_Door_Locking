/*
 * ESP32-S3 Door Lock Monitoring System
 * Based on A3144 Hall Sensor
 * 
 * Features:
 * - Hall sensor state monitoring
 * - WiFi connection with auto-reconnect
 * - MQTT communication for status reporting
 * - Non-blocking buzzer control
 * - LED status indication
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "config.h"
#include "wifi_manager.h"
#include "mqtt_client.h"
#include "hall_sensor.h"
#include "buzzer.h"

static const char *TAG = "DOOR_LOCK";

// Global state variables
static bool last_hall_state = true;  // HIGH = no magnet, LOW = magnet detected
static bool mqtt_connected = false;
static uint32_t last_mqtt_check = 0;

// Task handles
static TaskHandle_t hall_task_handle = NULL;
static TaskHandle_t mqtt_task_handle = NULL;

// Queue for MQTT messages
static QueueHandle_t mqtt_queue = NULL;

// MQTT message structure
typedef struct {
    char topic[64];
    char data[64];
    int len;
} mqtt_message_t;

/**
 * @brief Hall sensor state change callback
 */
static void hall_sensor_callback(bool state)
{
    ESP_LOGI(TAG, "Hall sensor state changed: %s", state ? "MAGNET_DETECTED" : "NO_MAGNET");
    
    if (state != last_hall_state) {
        last_hall_state = state;
        
        if (state) {
            // Magnet detected - Door locked
            ESP_LOGI(TAG, "🔒 Locked (Magnet detected)");
            
            // Publish MQTT message
            if (mqtt_is_connected()) {
                mqtt_publish(MQTT_TOPIC_STATE, "CLOSED", 6);
                ESP_LOGI(TAG, "Published: CLOSED");
            } else {
                ESP_LOGW(TAG, "MQTT not connected, message not sent");
            }
            
            // Start beep sequence
            buzzer_start_beep(BEEP_DEFAULT_TIMES, BEEP_DEFAULT_DURATION);
            
        } else {
            // No magnet - Door unlocked
            ESP_LOGI(TAG, "🔓 Unlocked (Magnet removed)");
            
            // Publish MQTT message
            if (mqtt_is_connected()) {
                mqtt_publish(MQTT_TOPIC_STATE, "OPEN", 4);
                ESP_LOGI(TAG, "Published: OPEN");
            } else {
                ESP_LOGW(TAG, "MQTT not connected, message not sent");
            }
            
            // Start beep sequence
            buzzer_start_beep(BEEP_DEFAULT_TIMES, BEEP_DEFAULT_DURATION);
        }
    }
}

/**
 * @brief MQTT message handler
 */
static void mqtt_message_handler(char* topic, char* data, int data_len)
{
    ESP_LOGI(TAG, "Received MQTT message [%s]: %.*s", topic, data_len, data);
    
    if (strcmp(topic, MQTT_TOPIC_CMD) == 0) {
        if (strncmp(data, "BEEP", 4) == 0) {
            ESP_LOGI(TAG, "Command received: BEEP");
            buzzer_start_beep(5, 300); // Beep 5 times for 300ms each
        } else if (strncmp(data, "STOP", 4) == 0) {
            ESP_LOGI(TAG, "Command received: STOP");
            buzzer_stop_beep();
        }
    }
}

/**
 * @brief MQTT connection status callback
 */
static void mqtt_connection_handler(bool connected)
{
    mqtt_connected = connected;
    if (connected) {
        ESP_LOGI(TAG, "MQTT connected");
        mqtt_subscribe(MQTT_TOPIC_CMD);
    } else {
        ESP_LOGW(TAG, "MQTT disconnected");
    }
}

/**
 * @brief MQTT monitoring task
 */
static void mqtt_task(void *pvParameters)
{
    mqtt_message_t msg;
    
    while (1) {
        // Check MQTT connection status
        uint32_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
        if (current_time - last_mqtt_check > MQTT_CHECK_INTERVAL_MS) {
            last_mqtt_check = current_time;
            
            if (!mqtt_is_connected()) {
                ESP_LOGW(TAG, "MQTT disconnected, attempting to reconnect...");
                mqtt_reconnect();
            }
        }
        
        // Process MQTT messages
        if (xQueueReceive(mqtt_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            mqtt_message_handler(msg.topic, msg.data, msg.len);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Hall sensor monitoring task
 */
static void hall_task(void *pvParameters)
{
    bool current_state;
    
    while (1) {
        current_state = hall_sensor_read();
        
        if (current_state != last_hall_state) {
            hall_sensor_callback(current_state);
        }
        
        vTaskDelay(pdMS_TO_TICKS(HALL_DEBOUNCE_MS));
    }
}

/**
 * @brief Initialize all components
 */
static esp_err_t init_components(void)
{
    esp_err_t ret;
    
    // Initialize WiFi
    ret = wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize MQTT
    ret = mqtt_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MQTT: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize Hall sensor
    ret = hall_sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Hall sensor: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize buzzer
    ret = buzzer_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize buzzer: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

/**
 * @brief Create tasks
 */
static esp_err_t create_tasks(void)
{
    // Create MQTT queue
    mqtt_queue = xQueueCreate(10, sizeof(mqtt_message_t));
    if (mqtt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create MQTT queue");
        return ESP_FAIL;
    }
    
    // Create MQTT task
    BaseType_t ret = xTaskCreate(mqtt_task, "mqtt_task", MQTT_TASK_STACK_SIZE, 
                                NULL, MQTT_TASK_PRIORITY, &mqtt_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MQTT task");
        return ESP_FAIL;
    }
    
    // Create Hall sensor task
    ret = xTaskCreate(hall_task, "hall_task", HALL_TASK_STACK_SIZE, 
                     NULL, HALL_TASK_PRIORITY, &hall_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Hall sensor task");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Starting Door Lock Monitoring System");
    ESP_LOGI(TAG, "ESP-IDF version: %s", esp_get_idf_version());
    
    // Initialize all components
    if (init_components() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize components");
        return;
    }
    
    // Set Hall sensor callback
    hall_sensor_set_callback(hall_sensor_callback);
    
    // Create tasks
    if (create_tasks() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create tasks");
        return;
    }
    
    ESP_LOGI(TAG, "System initialized successfully");
    ESP_LOGI(TAG, "Waiting for Hall sensor state changes...");
    
    // Main loop - handle buzzer updates
    while (1) {
        buzzer_update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
