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
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

// ESP-IDF MQTT header
#include "mqtt_client.h"

#include "config.h"
#include "wifi_manager.h"
#include "hall_sensor.h"
#include "buzzer.h"

static const char *TAG = "DOOR_LOCK";

// Global state variables
static bool last_hall_state = true;  // HIGH = no magnet, LOW = magnet detected
static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

// ----------------- MQTT callback -----------------
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to MQTT broker");
            mqtt_connected = true;
            
            // Subscribe to command topic
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_CMD, 0);
            ESP_LOGI(TAG, "Subscribed to topic: %s", MQTT_TOPIC_CMD);
            
            // Turn on LED to indicate MQTT connection
            gpio_set_level(LED_PIN, 1);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from MQTT broker");
            mqtt_connected = false;
            gpio_set_level(LED_PIN, 0);
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "--- Get MQTT Message - Topic: %.*s, Data: %.*s", 
                     event->topic_len, event->topic, 
                     event->data_len, event->data);
            
            // Handle BEEP command
            if (strncmp(event->data, "BEEP", event->data_len) == 0) {
                ESP_LOGI(TAG, "Command: BEEP");
                buzzer_start_beep(5, 300);
            }
            // Handle STOP command
            else if (strncmp(event->data, "STOP", event->data_len) == 0) {
                ESP_LOGI(TAG, "Command: STOP");
                buzzer_stop_beep();
            }
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
            
        default:
            break;
    }
}

// ----------------- MQTT initialization -----------------
static esp_err_t mqtt_init(void)
{
    char mqtt_uri[64];
    snprintf(mqtt_uri, sizeof(mqtt_uri), "mqtt://%s:%d", MQTT_SERVER, MQTT_PORT);
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = mqtt_uri,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .session.keepalive = 60,
    };
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }
    
    // Register event handler
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    // Start MQTT client
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "MQTT client initialized and started");
    return ESP_OK;
}

// ----------------- Hall sensor task -----------------
static void hall_task(void *pvParameters)
{
    bool current_state;
    static uint32_t last_interrupt_time = 0;
    
    while (1) {
        current_state = hall_sensor_read();
        
        if (current_state != last_hall_state) {
            uint32_t current_time = esp_timer_get_time() / 1000;
            
            // Debounce check
            if (current_time - last_interrupt_time > HALL_DEBOUNCE_MS) {
                last_hall_state = current_state;
                last_interrupt_time = current_time;
                
                ESP_LOGI(TAG, "Hall sensor state changed: %s", 
                         current_state ? "MAGNET_DETECTED" : "NO_MAGNET");
                
                if (!current_state) {
                    // Magnet detected - Door locked
                    ESP_LOGI(TAG, "Door CLOSED (Locked)");
                    
                    // Publish MQTT message
                    if (mqtt_connected && mqtt_client) {
                        esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_STATE, "CLOSED", 6, 1, 0);
                        ESP_LOGI(TAG, "Published: CLOSED");
                    }
                    
                    // Beep to indicate door closed
                    buzzer_start_beep(BEEP_DEFAULT_TIMES, BEEP_DEFAULT_DURATION);
                    
                } else {
                    // No magnet - Door unlocked
                    ESP_LOGI(TAG, "Door OPEN (Unlocked)");
                    
                    // Publish MQTT message
                    if (mqtt_connected && mqtt_client) {
                        esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_STATE, "OPEN", 4, 1, 0);
                        ESP_LOGI(TAG, "Published: OPEN");
                    }
                    
                    // Beep to indicate door opened
                    buzzer_start_beep(BEEP_DEFAULT_TIMES, BEEP_DEFAULT_DURATION);
                }
                
                // Re-enable interrupt after processing
                hall_sensor_re_enable_interrupt();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ----------------- Initialize all components -----------------
static esp_err_t init_components(void)
{
    esp_err_t ret;
    
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize WiFi
    ret = wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait a bit for WiFi to connect
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Initialize MQTT
    ret = mqtt_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize MQTT: %s", esp_err_to_name(ret));
        // Don't return error, continue without MQTT
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

// ----------------- Create tasks -----------------
static esp_err_t create_tasks(void)
{
    // Create Hall sensor task
    BaseType_t ret = xTaskCreate(hall_task, "hall_task", HALL_TASK_STACK_SIZE, 
                                 NULL, HALL_TASK_PRIORITY, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Hall sensor task");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

// ----------------- Main entry function -----------------
void app_main(void)
{
    ESP_LOGI(TAG, "Starting Door Lock Monitoring System");
    ESP_LOGI(TAG, "ESP-IDF version: %s", esp_get_idf_version());
    
    // Initialize all components
    if (init_components() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize components");
        return;
    }
    
    // Create tasks
    if (create_tasks() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create tasks");
        return;
    }

    // Enable WiFi power save (highest savings while connected)
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    
    ESP_LOGI(TAG, "System initialized successfully");
    ESP_LOGI(TAG, "Monitoring Hall sensor...");
    
    // Main loop - handle buzzer updates
    while (1) {
        buzzer_update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
