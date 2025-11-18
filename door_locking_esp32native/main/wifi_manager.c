#include "wifi_manager.h"
#include "config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"

static const char *TAG = "WIFI_MANAGER";

// WiFi event group
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static esp_netif_t *s_netif = NULL;
static TaskHandle_t s_reconnect_task_handle = NULL;

#define MONITOR_CHECK_INTERVAL 60000   // Monitor checks every 60 seconds as fallback

/**
 * @brief Background task to monitor WiFi and auto-reconnect
 * This serves as a fallback mechanism - the event handler does the primary reconnection
 */
static void wifi_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "WiFi monitor task started (60s interval for fallback)");
    
    while (1) {
        // Check every 60 seconds as a fallback (event handler does fast reconnect)
        vTaskDelay(pdMS_TO_TICKS(MONITOR_CHECK_INTERVAL));
        
        if (!wifi_is_connected()) {
            ESP_LOGW(TAG, "Monitor: WiFi still disconnected after 60s, forcing reconnect...");
            esp_wifi_connect();
        }
    }
}

/**
 * @brief WiFi event handler with auto-reconnect
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Turn off LED when WiFi disconnects
        gpio_set_level(LED_PIN, 0);
        
        // Clear connected bit
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        
        ESP_LOGW(TAG, "Disconnected from AP, attempting immediate reconnect...");
        
        // Immediate reconnect attempt (ESP-IDF will handle retry logic)
        esp_wifi_connect();
        
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        
        // Turn on LED when connected
        gpio_set_level(LED_PIN, 1);
        
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
}

esp_err_t wifi_init(void)
{
    // Configure LED pin using new GPIO driver
    gpio_config_t led_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED_PIN, 0);  // Start with LED off
    
    // Initialize TCP/IP adapter
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    s_netif = esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Create event group
    s_wifi_event_group = xEventGroupCreate();
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    
    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    
    // Start WiFi first
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Set WiFi power after starting
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(44)); // 11 dBm = 44 * 0.25 dBm (low power mode)
    
    ESP_LOGI(TAG, "WiFi initialization finished");
    ESP_LOGI(TAG, "Connecting to %s...", WIFI_SSID);
    
    // Wait for initial connection (with timeout)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                          WIFI_CONNECTED_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(10000)); // 10 second timeout
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi SSID:%s", WIFI_SSID);
        // Turn on LED to indicate WiFi connection
        gpio_set_level(LED_PIN, 1);
    } else {
        ESP_LOGW(TAG, "Initial connection failed, will retry in background");
    }
    
    // Start background monitoring task (will keep retrying forever)
    BaseType_t ret = xTaskCreate(wifi_monitor_task, 
                                 "wifi_monitor", 
                                 3072,  // Stack size
                                 NULL, 
                                 3,     // Priority
                                 &s_reconnect_task_handle);
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WiFi monitor task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "WiFi monitor task created - will auto-reconnect indefinitely");
    return ESP_OK;
}

bool wifi_is_connected(void)
{
    if (s_wifi_event_group == NULL) {
        return false;
    }
    
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

const char* wifi_get_ip_address(void)
{
    if (!wifi_is_connected()) {
        return NULL;
    }
    
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(s_netif, &ip_info) == ESP_OK) {
        static char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
        return ip_str;
    }
    
    return NULL;
}

// esp_err_t wifi_reconnect(void)
// {
//     if (wifi_is_connected()) {
//         return ESP_OK;
//     }
    
//     ESP_LOGI(TAG, "Reconnecting to WiFi...");
//     s_retry_num = 0;
//     esp_wifi_connect();
    
//     // Wait for connection
//     EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
//                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
//                                           pdFALSE,
//                                           pdFALSE,
//                                           pdMS_TO_TICKS(10000)); // 10 second timeout
    
//     if (bits & WIFI_CONNECTED_BIT) {
//         ESP_LOGI(TAG, "Reconnected to WiFi");
//         // Turn on LED to indicate WiFi reconnection
//         gpio_set_level(LED_PIN, 1);
//         return ESP_OK;
//     } else {
//         ESP_LOGW(TAG, "Failed to reconnect to WiFi");
//         return ESP_FAIL;
//     }
// }

esp_err_t wifi_manager_deinit(void)
{
    // Stop monitor task
    if (s_reconnect_task_handle) {
        vTaskDelete(s_reconnect_task_handle);
        s_reconnect_task_handle = NULL;
    }
    
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
    
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler));
    
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    
    return ESP_OK;
}
