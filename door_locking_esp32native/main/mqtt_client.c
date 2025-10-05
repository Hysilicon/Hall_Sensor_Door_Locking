#include "mqtt_client.h"
#include "config.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "MQTT_CLIENT";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static SemaphoreHandle_t mqtt_mutex = NULL;
static bool mqtt_connected_flag = false;

// Forward declarations
static void mqtt_event_handler(esp_mqtt_event_handle_t event);
static void mqtt_publish_task(void *pvParameters);

/**
 * @brief MQTT event handler
 */
static void mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt_connected_flag = true;
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            mqtt_connected_flag = false;
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
            
            // Forward message to main application
            // This would typically be handled by a callback or queue
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            mqtt_connected_flag = false;
            break;
            
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

esp_err_t mqtt_init(void)
{
    // Create mutex for thread safety
    mqtt_mutex = xSemaphoreCreateMutex();
    if (mqtt_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create MQTT mutex");
        return ESP_FAIL;
    }
    
    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://" MQTT_SERVER ":" STRINGIFY(MQTT_PORT),
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .session.keepalive = 60,
        .session.disable_clean_session = false,
        .network.timeout_ms = 10000,
        .network.reconnect_timeout_ms = 10000,
        .network.refresh_connection_after_ms = 1000,
    };
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        vSemaphoreDelete(mqtt_mutex);
        return ESP_FAIL;
    }
    
    // Register event handler
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    
    // Start MQTT client
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(mqtt_client);
        vSemaphoreDelete(mqtt_mutex);
        return ret;
    }
    
    ESP_LOGI(TAG, "MQTT client initialized");
    return ESP_OK;
}

bool mqtt_is_connected(void)
{
    if (mqtt_mutex == NULL) {
        return false;
    }
    
    xSemaphoreTake(mqtt_mutex, portMAX_DELAY);
    bool connected = mqtt_connected_flag;
    xSemaphoreGive(mqtt_mutex);
    
    return connected;
}

esp_err_t mqtt_publish(const char* topic, const char* data, int len)
{
    if (mqtt_client == NULL || !mqtt_is_connected()) {
        ESP_LOGW(TAG, "MQTT client not connected, cannot publish");
        return ESP_FAIL;
    }
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, data, len, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish message");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Published message, msg_id=%d", msg_id);
    return ESP_OK;
}

esp_err_t mqtt_subscribe(const char* topic)
{
    if (mqtt_client == NULL || !mqtt_is_connected()) {
        ESP_LOGW(TAG, "MQTT client not connected, cannot subscribe");
        return ESP_FAIL;
    }
    
    int msg_id = esp_mqtt_client_subscribe(mqtt_client, topic, 1);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Subscribed to topic: %s, msg_id=%d", topic, msg_id);
    return ESP_OK;
}

esp_err_t mqtt_reconnect(void)
{
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Reconnecting to MQTT broker...");
    
    // Stop and restart the client
    esp_mqtt_client_stop(mqtt_client);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second
    
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to restart MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

esp_err_t mqtt_deinit(void)
{
    if (mqtt_client) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }
    
    if (mqtt_mutex) {
        vSemaphoreDelete(mqtt_mutex);
        mqtt_mutex = NULL;
    }
    
    mqtt_connected_flag = false;
    return ESP_OK;
}
