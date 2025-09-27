#include <Arduino.h>
#include <WiFi.h>
#include <AsyncMQTT_ESP32.h>

#define HALL_PIN   5   // Connect pin 2 to hall sensor
#define BUZZER_PIN 12  // Connect pin 12 to buzzer

// Wi-Fi information
const char* ssid = "";
const char* password = "";

// MQTT Broker information
const char* mqtt_server = "";
const int mqtt_port = 1883;
const char* mqtt_username = "";
const char* mqtt_password = "";

// Async MQTT client
AsyncMqttClient mqttClient;

bool lastState = HIGH;  // Hall sensor state
bool beepActive = false; // Buzzer state machine
int beepTimes = 0;
int beepDuration = 0; // Buzzer duration in milliseconds
unsigned long beepStartTime = 0;

// MQTT connection monitoring
unsigned long lastMqttCheck = 0;
bool mqttConnected = false;
unsigned long mqttConnectStartTime = 0;
bool mqttConnecting = false;

void setup() {
  Serial.begin(115200);
  pinMode(HALL_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Set up Wi-Fi
  setup_wifi();
  
  // Set up MQTT callbacks
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  
  // Set MQTT server and credentials
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCredentials(mqtt_username, mqtt_password);
}

void loop() {
  // Check MQTT connection timeout (10 seconds)
  if (mqttConnecting && (millis() - mqttConnectStartTime > 10000)) {
    Serial.println("MQTT connection timeout!");
    mqttConnecting = false;
    if (WiFi.isConnected()) {
      Serial.println("Retrying MQTT connection...");
      mqttConnectStartTime = millis();
      mqttClient.connect();
    }
  }

  // Check MQTT connection status every 5 seconds
  if (millis() - lastMqttCheck > 5000) {
    lastMqttCheck = millis();
    bool currentMqttStatus = mqttClient.connected();
    if (currentMqttStatus != mqttConnected) {
      mqttConnected = currentMqttStatus;
      mqttConnecting = false; // Reset connecting flag when status changes
      if (mqttConnected) {
        Serial.println("MQTT Status: Connected");
      } else {
        Serial.println("MQTT Status: Disconnected");
        if (WiFi.isConnected()) {
          Serial.println("Attempting to reconnect to MQTT...");
          mqttConnectStartTime = millis();
          mqttConnecting = true;
          mqttClient.connect();
        }
      }
    }
  }

  // Read hall sensor
  bool hallState = digitalRead(HALL_PIN);

  if (hallState != lastState) {
    lastState = hallState;

    if (hallState == LOW) {
      Serial.println("🔒 Locked (Magnet detected)");
      if (mqttClient.connected()) {
        mqttClient.publish("esp32/lock/state", 1, true, "CLOSED");
      } else {
        Serial.println("MQTT not connected, message not sent");
      }
      startBeep(3, 200);             // Non-blocking beep three times when locked
    } else {
      Serial.println("🔓 Unlocked (Magnet removed)");
      if (mqttClient.connected()) {
        mqttClient.publish("esp32/lock/state", 1, true, "OPEN");
      } else {
        Serial.println("MQTT not connected, message not sent");
      }
      startBeep(3, 200);             // Non-blocking beep three times when unlocked
    }
  }

  // Non-blocking buzzer handling
  handleBeep();
  delay(100); // Debounce
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connect WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  WiFi.setTxPower(WIFI_POWER_2dBm);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Connect to MQTT broker after WiFi is connected
  Serial.println("Attempting to connect to MQTT broker...");
  Serial.print("MQTT Server: ");
  Serial.print(mqtt_server);
  Serial.print(":");
  Serial.println(mqtt_port);
  Serial.print("Username: ");
  Serial.println(mqtt_username);
  
  // Add a small delay before connecting
  delay(1000);
  mqttConnectStartTime = millis();
  mqttConnecting = true;
  mqttClient.connect();
}

// MQTT callback functions
void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT broker!");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  mqttConnecting = false; // Reset connecting flag
  mqttClient.subscribe("esp32/lock/cmd", 1);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.print("Disconnected from MQTT broker. Reason: ");
  Serial.println((int8_t)reason);
  
  if (WiFi.isConnected()) {
    Serial.println("WiFi still connected, attempting to reconnect to MQTT...");
    delay(5000);
    mqttClient.connect();
  } else {
    Serial.println("WiFi disconnected, will reconnect when WiFi is available.");
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.print("Received message [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < len; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
}

// Start buzzer state machine
void startBeep(int times, int duration) {
  beepTimes = times * 2; // Each HIGH+LOW counts as two steps
  beepDuration = duration;
  beepStartTime = millis();
  beepActive = true;
}

// Non-blocking buzzer control
void handleBeep() {
  if (!beepActive) return;

  unsigned long current = millis();
  if (current - beepStartTime >= beepDuration) {
    // Switch buzzer state
    digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
    beepStartTime = current;
    beepTimes--;
    if (beepTimes <= 0) {
      beepActive = false;
      digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off
    }
  }
}