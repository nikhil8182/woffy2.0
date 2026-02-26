/*
 * Woffy 2.0 — MQTT Only (Robust)
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "motors.h"

bool mqttConnected = false;
bool wifiConnected = false;
String lastCommand = "";

// ===== LED =====
#define LED_PIN 2

void setLED(bool on) {
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void updateLED() {
  if (wifiConnected && mqttConnected) {
    setLED(true); // Solid = all good
  } else {
    setLED(millis() % 1000 < 500); // Blink = not working
  }
}

// ===== MQTT =====
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String cmd = String((char*)payload);
  cmd.trim();
  Serial.print("MQTT RX: ");
  Serial.println(cmd);
  Serial.println("--------------------");
  executeCommand(cmd);
}

bool connectMQTT() {
  if (!MQTT_ENABLED) return false;

  Serial.print("MQTT: ");
  Serial.println(MQTT_BROKER);

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  String clientId = String(MQTT_CLIENT_ID) + "_" + String(random(1000, 9999));

  if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("MQTT: Connected");
    mqttClient.subscribe(MQTT_TOPIC_CMD);
    return true;
  }
  Serial.print("MQTT: Failed ");
  Serial.println(mqttClient.state());
  return false;
}

void setupMQTT() {
  if (!MQTT_ENABLED) return;
  mqttConnected = connectMQTT();
}

void mqttReconnect() {
  if (!MQTT_ENABLED) return;
  if (!mqttClient.connected()) {
    mqttConnected = connectMQTT();
  }
}

void publishStatus(const String& status) {
  if (mqttConnected) {
    mqttClient.publish(MQTT_TOPIC_STATUS, status.c_str());
  }
}

// ===== Commands =====
int currentSpeed = DEFAULT_SPEED;

void executeCommand(const String& cmd) {
  lastCommand = cmd;

  if (cmd == "demo" || cmd == "sequence") {
    Serial.println("Demo: 2s fwd, wait, 5s fwd");
    moveForward(currentSpeed);
    delay(2000);
    stopAll();
    delay(1000);
    moveForward(currentSpeed);
    delay(5000);
    stopAll();
    publishStatus("DemoDone");
    return;
  }

  if (cmd.startsWith("speed:")) {
    currentSpeed = cmd.substring(6).toInt();
    if (currentSpeed < 60) currentSpeed = 60;
    if (currentSpeed > 255) currentSpeed = 255;
    Serial.print("Speed: ");
    Serial.println(currentSpeed);
    publishStatus("Speed:" + String(currentSpeed));
    return;
  }

  if (cmd == "fwd" || cmd == "forward" || cmd == "w") {
    moveForward(currentSpeed);
    publishStatus("Forward");
  }
  else if (cmd == "bwd" || cmd == "backward" || cmd == "s") {
    moveBackward(currentSpeed);
    publishStatus("Backward");
  }
  else if (cmd == "left" || cmd == "a") {
    turnLeft(currentSpeed);
    publishStatus("Left");
  }
  else if (cmd == "right" || cmd == "d") {
    turnRight(currentSpeed);
    publishStatus("Right");
  }
  else if (cmd == "stop" || cmd == "x" || cmd == " ") {
    stopAll();
    publishStatus("Stopped");
  }
  else if (cmd == "status") {
    String status = "IP:" + WiFi.localIP().toString() +
                    "|WiFi:" + String(wifiConnected ? "1" : "0") +
                    "|MQTT:" + String(mqttConnected ? "1" : "0");
    publishStatus(status);
  }
  else {
    Serial.print("Unknown: ");
    Serial.println(cmd);
  }
}

// ===== Serial =====
void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  Serial.print("> ");
  Serial.println(cmd);

  executeCommand(cmd);
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(500); // Wait for serial

  Serial.println();
  Serial.println("========================");
  Serial.println("WOFFY 2.0 - MQTT ROBUST");
  Serial.println("========================");
  Serial.println("Changelog v2:");
  Serial.println("  - Smooth motor ramping (no instant reversals)");
  Serial.println("  - Fixed LED polarity (HIGH=on)");
  Serial.println("  - Fixed front motor direction (FL/FR swapped)");
  Serial.println("  - Fixed stale MQTT state detection");
  Serial.println("  - Serial uses shared executeCommand");
  Serial.println("========================");

  pinMode(LED_PIN, OUTPUT);
  setLED(false);

  setupMotors();

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WOFFY_SSID, WOFFY_PASS);

  Serial.print("WiFi: ");
  Serial.println(WOFFY_SSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.print("WiFi: OK ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi: FAILED!");
  }

  setupMQTT();

  Serial.println();
  Serial.println("========== READY ==========");
}

unsigned long lastWifiPub = 0;
unsigned long lastReconnect = 0;

void loop() {
  handleSerial();
  updateMotors();
  updateLED();

  if (MQTT_ENABLED) {
    // Sync flag with actual connection state
    if (mqttConnected && !mqttClient.connected()) {
      mqttConnected = false;
      Serial.println("MQTT: Connection lost");
    }

    if (mqttConnected) {
      mqttClient.loop();

      // Publish status every 30s
      if (millis() - lastWifiPub > 30000) {
        String info = "IP:" + WiFi.localIP().toString() +
                     "|RSSI:" + String(WiFi.RSSI());
        mqttClient.publish(MQTT_TOPIC_STATUS, info.c_str());
        lastWifiPub = millis();
      }
    } else {
      // Try reconnect every 5 seconds
      if (millis() - lastReconnect > 5000) {
        lastReconnect = millis();
        mqttReconnect();
        if (mqttConnected) {
          Serial.println("MQTT: Reconnected!");
        }
      }
    }
  }
}
