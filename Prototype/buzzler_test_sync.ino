#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define HALL_PIN   5   // Connect pin 5 to hall sensor
#define BUZZER_PIN 12  // Connect pin 12 to buzzer

// Wi-Fi information
const char* ssid = "";
const char* password = "";

// MQTT Broker information
const char* mqtt_server = ""; 
const int mqtt_port = 1883;
const char* mqtt_username = "";
const char* mqtt_password = "";

// Establish WiFi and MQTT client
WiFiClient espClient;
PubSubClient client(espClient);

bool lastState = HIGH;  // Hall sensor state
bool beepActive = false; // Buzzer state machine
int beepTimes = 0;
int beepDuration = 0; // Buzzer duration in milliseconds
unsigned long beepStartTime = 0;

// MQTT connection monitoring
unsigned long lastMqttCheck = 0;
bool mqttConnected = false;

void setup() {
  Serial.begin(115200);
  pinMode(HALL_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Set up Wi-Fi
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  // Check MQTT connection status every 5 seconds
  if (millis() - lastMqttCheck > 5000) {
    lastMqttCheck = millis();
    if (!client.connected()) {
      Serial.println("MQTT disconnected, attempting to reconnect...");
      reconnect();
    }
  }

  // Process MQTT messages
  client.loop();

  // Read hall sensor
  bool hallState = digitalRead(HALL_PIN);

  if (hallState != lastState) {
    lastState = hallState;

    if (hallState == LOW) {
      Serial.println("🔒 Locked (Magnet detected)");
      if (client.connected()) {
        client.publish("esp32/lock/state", "CLOSED");
        Serial.println("Published: CLOSED");
      } else {
        Serial.println("MQTT not connected, message not sent");
      }
      startBeep(3, 200);             // Non-blocking beep three times when locked
    } else {
      Serial.println("🔓 Unlocked (Magnet removed)");
      if (client.connected()) {
        client.publish("esp32/lock/state", "OPEN");
        Serial.println("Published: OPEN");
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
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    Serial.print("Server: ");
    Serial.print(mqtt_server);
    Serial.print(":");
    Serial.println(mqtt_port);
    Serial.print("Username: ");
    Serial.println(mqtt_username);
    
    // Attempt to connect
    if (client.connect("ESP32Client", mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT broker!");
      // Subscribe to command topic
      client.subscribe("esp32/lock/cmd");
      Serial.println("Subscribed to esp32/lock/cmd");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" - Retrying in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Received message [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
  }
  Serial.println();
  
  // Handle different commands
  String messageStr = "";
  for (int i = 0; i < length; i++) {
    messageStr += (char)message[i];
  }
  
  if (String(topic) == "esp32/lock/cmd") {
    if (messageStr == "BEEP") {
      Serial.println("Command received: BEEP");
      startBeep(5, 300); // Beep 5 times for 300ms each
    } else if (messageStr == "STOP") {
      Serial.println("Command received: STOP");
      beepActive = false;
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
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
