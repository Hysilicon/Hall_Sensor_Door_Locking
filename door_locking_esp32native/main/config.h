#ifndef CONFIG_H
#define CONFIG_H

// Hardware pin definitions
#define HALL_PIN      5   // GPIO5 for Hall sensor (A3144)
#define BUZZER_PIN    12  // GPIO12 for active buzzer
#define LED_PIN       2   // GPIO2 for status LED

// WiFi configuration
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// MQTT configuration
#define MQTT_SERVER   "YOUR_MQTT_SERVER"
#define MQTT_PORT     1883
#define MQTT_USERNAME "YOUR_MQTT_USERNAME"
#define MQTT_PASSWORD "YOUR_MQTT_PASSWORD"
#define MQTT_CLIENT_ID "ESP32_DoorLock"

// MQTT topics
#define MQTT_TOPIC_STATE "esp32/lock/state"
#define MQTT_TOPIC_CMD   "esp32/lock/cmd"

// Timing configuration
#define MQTT_CHECK_INTERVAL_MS  5000  // Check MQTT connection every 5 seconds
#define HALL_DEBOUNCE_MS        100   // Hall sensor debounce time
#define BEEP_DEFAULT_TIMES      3     // Default beep times
#define BEEP_DEFAULT_DURATION   200   // Default beep duration in ms

// Task priorities
#define WIFI_TASK_PRIORITY      5
#define MQTT_TASK_PRIORITY      4
#define HALL_TASK_PRIORITY      6
#define BUZZER_TASK_PRIORITY    3

// Task stack sizes
#define WIFI_TASK_STACK_SIZE    4096
#define MQTT_TASK_STACK_SIZE    4096
#define HALL_TASK_STACK_SIZE    2048
#define BUZZER_TASK_STACK_SIZE  2048

#endif // CONFIG_H
