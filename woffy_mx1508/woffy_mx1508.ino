/*
 * Woffy 2.0 — Performance Edition
 * MQTT + HTTP fast path + Servo + Ultrasonic
 */

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "sensors.h"
#include "motors.h"
#include "html.h"

bool mqttConnected = false;
bool wifiConnected = false;
String lastCommand = "";
int currentSpeed = DEFAULT_SPEED;

// ===== HTTP Server (fast path) =====
WebServer server(80);

void handleRoot() {
  server.send_P(200, "text/html", CONTROL_HTML);
}

void handleCmd() {
  String cmd = server.arg("cmd");
  if (cmd.length() > 0) {
    Serial.print("HTTP RX: ");
    Serial.println(cmd);
    executeCommand(cmd);
    server.send(200, "text/plain", "OK:" + cmd);
  } else {
    server.send(400, "text/plain", "No cmd");
  }
}

void handleWifiStatus() {
  String json = "{";
  json += "\"sta_connected\":" + String(wifiConnected ? "true" : "false");
  json += ",\"sta_ssid\":\"" + String(WOFFY_SSID) + "\"";
  json += ",\"sta_ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"mqtt_connected\":" + String(mqttConnected ? "true" : "false");
  json += ",\"rssi\":" + String(WiFi.RSSI());
  json += ",\"uptime\":" + String(millis() / 1000);
  json += "}";
  server.send(200, "application/json", json);
}

void handleStatus() {
  float dist = getDistance();
  readSensors();
  String json = "{";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"wifi\":" + String(wifiConnected ? "true" : "false");
  json += ",\"mqtt\":" + String(mqttConnected ? "true" : "false");
  json += ",\"speed\":" + String(currentSpeed);
  json += ",\"servo\":" + String(servoAngle);
  json += ",\"distance\":" + String(dist, 1);
  json += ",\"rssi\":" + String(WiFi.RSSI());
  json += ",\"uptime\":" + String(millis() / 1000);
  json += ",\"last_cmd\":\"" + lastCommand + "\"";
  json += ",\"sensors\":{";
  json += "\"fc\":" + String(sensorMM[S_FRONT_C]);
  json += ",\"fl\":" + String(sensorMM[S_FRONT_L]);
  json += ",\"fr\":" + String(sensorMM[S_FRONT_R]);
  json += ",\"cf\":" + String(sensorMM[S_CLIFF_F]);
  json += ",\"l\":" + String(sensorMM[S_LEFT]);
  json += ",\"r\":" + String(sensorMM[S_RIGHT]);
  json += ",\"bc\":" + String(sensorMM[S_BACK_C]);
  json += ",\"cb\":" + String(sensorMM[S_CLIFF_B]);
  json += ",\"active\":" + String(sensorsActive);
  json += "}";
  json += "}";
  server.send(200, "application/json", json);
}

void handleRadarPoint() {
  // Single radar reading at current servo angle
  float dist = getDistance();
  if (dist <= 0 || dist > 100) dist = 100;
  String json = "{\"angle\":" + String(servoAngle) + ",\"distance\":" + String(dist, 1) + "}";
  server.send(200, "application/json", json);
}

void handleTof() {
  readSensors();
  String json = "{";
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (i > 0) json += ",";
    json += "\"" + String(sensorNames[i]) + "\":" + String(sensorMM[i]);
  }
  json += ",\"active\":" + String(sensorsActive);
  json += "}";
  server.send(200, "application/json", json);
}

void setupHTTPServer() {
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/wifi/status", handleWifiStatus);
  server.on("/status", handleStatus);
  server.on("/radar", handleRadarPoint);
  server.on("/tof", handleTof);
  server.begin();
  Serial.print("HTTP: http://");
  Serial.println(WiFi.localIP());
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
  executeCommand(cmd);
}

bool connectMQTT() {
  if (!MQTT_ENABLED) return false;

  Serial.print("MQTT: ");
  Serial.println(MQTT_BROKER);

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(10);        // 10s keepalive (faster disconnect detection)
  mqttClient.setSocketTimeout(3);     // 3s socket timeout

  String clientId = String(MQTT_CLIENT_ID) + "_" + String(random(1000, 9999));

  if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
    Serial.println("MQTT: Connected");
    mqttClient.subscribe(MQTT_TOPIC_CMD, 0);  // QoS 0 — fire and forget
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

// ===== Radar publisher =====
void radarPublish(int angle, float distance) {
  // angle=-1 means sweep complete
  if (angle < 0) {
    publishStatus("RadarDone");
    Serial.println("Radar: done");
    return;
  }
  // Format: "R:angle:distance" — compact for fast streaming
  String msg = "R:" + String(angle) + ":" + String(distance, 1);
  publishStatus(msg);
  Serial.print("R:");
  Serial.print(angle);
  Serial.print(":");
  Serial.println(distance, 1);
}

// ===== Autopilot publisher =====
void autoStatusPublish(const String& status) {
  publishStatus(status);
  Serial.println(status);
}

// ===== Non-blocking Demo =====
enum DemoState { DEMO_IDLE, DEMO_FWD1, DEMO_PAUSE, DEMO_FWD2 };
DemoState demoState = DEMO_IDLE;
unsigned long demoStart = 0;

void startDemo() {
  demoState = DEMO_FWD1;
  demoStart = millis();
  moveForward(currentSpeed);
  Serial.println("Demo: started");
}

void updateDemo() {
  if (demoState == DEMO_IDLE) return;

  unsigned long elapsed = millis() - demoStart;

  switch (demoState) {
    case DEMO_FWD1:
      if (elapsed >= 2000) {
        stopAll();
        demoState = DEMO_PAUSE;
        demoStart = millis();
      }
      break;
    case DEMO_PAUSE:
      if (elapsed >= 1000) {
        moveForward(currentSpeed);
        demoState = DEMO_FWD2;
        demoStart = millis();
      }
      break;
    case DEMO_FWD2:
      if (elapsed >= 5000) {
        stopAll();
        demoState = DEMO_IDLE;
        publishStatus("DemoDone");
        Serial.println("Demo: done");
      }
      break;
    default:
      break;
  }
}

// ===== Commands =====
void executeCommand(const String& cmd) {
  lastCommand = cmd;

  // Autopilot commands
  if (cmd == "autopilot" || cmd == "auto" || cmd == "selfdrv") {
    if (isRadarActive()) stopRadar();
    startAutopilot();
    return;
  }
  if (cmd == "autopilot:stop" || cmd == "auto:stop" || cmd == "selfdrv:stop") {
    stopAutopilot();
    return;
  }

  // Any manual movement command cancels autopilot
  if (isAutopilotActive()) {
    if (cmd == "fwd" || cmd == "forward" || cmd == "w" ||
        cmd == "bwd" || cmd == "backward" || cmd == "s" ||
        cmd == "left" || cmd == "a" ||
        cmd == "right" || cmd == "d" ||
        cmd == "stop" || cmd == "x") {
      stopAutopilot();
      Serial.println("Autopilot: cancelled by manual command");
    }
  }

  if (cmd == "demo" || cmd == "sequence") {
    startDemo();
    return;
  }

  if (cmd.startsWith("speed:")) {
    if (isAutopilotActive()) {
      Serial.println("Speed locked in autopilot mode");
      publishStatus("Speed:locked");
      return;
    }
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
    demoState = DEMO_IDLE;  // cancel demo if running
    publishStatus("Stopped");
  }
  else if (cmd == "status") {
    float dist = getDistance();
    String status = "IP:" + WiFi.localIP().toString() +
                    "|WiFi:" + String(wifiConnected ? "1" : "0") +
                    "|MQTT:" + String(mqttConnected ? "1" : "0") +
                    "|Dist:" + String(dist, 1) + "cm" +
                    "|Servo:" + String(servoAngle);
    publishStatus(status);
  }
  // Servo commands
  else if (cmd.startsWith("servo:")) {
    int angle = cmd.substring(6).toInt();
    setServo(angle);
    Serial.print("Servo: ");
    Serial.println(angle);
    publishStatus("Servo:" + String(angle));
  }
  else if (cmd == "look:left") {
    setServo(180);
    publishStatus("Servo:180");
  }
  else if (cmd == "look:right") {
    setServo(0);
    publishStatus("Servo:0");
  }
  else if (cmd == "look:center") {
    setServo(90);
    publishStatus("Servo:90");
  }
  // Ultrasonic commands
  else if (cmd == "dist" || cmd == "distance") {
    float dist = getDistance();
    Serial.print("Distance: ");
    Serial.print(dist, 1);
    Serial.println(" cm");
    publishStatus("Dist:" + String(dist, 1) + "cm");
  }
  else if (cmd == "scan") {
    scanSweep();
    publishStatus("ScanDone");
  }
  // Radar commands
  else if (cmd == "radar" || cmd == "radar:once") {
    startRadar(false);  // single sweep
  }
  else if (cmd == "radar:continuous" || cmd == "radar:on") {
    startRadar(true);   // continuous sweeping
  }
  else if (cmd == "radar:stop" || cmd == "radar:off") {
    stopRadar();
    publishStatus("RadarDone");
  }
  // ToF sensor commands
  else if (cmd == "tof") {
    readSensors();
    printSensors();
    publishStatus(sensorString());
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

// ===== Wi-Fi Recovery (non-blocking) =====
unsigned long lastWifiCheck = 0;

void checkWifiConnection() {
  if (millis() - lastWifiCheck < 10000) return;  // check every 10s
  lastWifiCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      wifiConnected = false;
      Serial.println("WiFi: Lost connection, reconnecting...");
    }
    WiFi.reconnect();
  } else if (!wifiConnected) {
    wifiConnected = true;
    Serial.print("WiFi: Reconnected ");
    Serial.println(WiFi.localIP());
  }
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("======================================");
  Serial.println("WOFFY 2.0 — PERFORMANCE EDITION");
  Serial.println("======================================");
  Serial.println("  - 20kHz PWM (silent motors)");
  Serial.println("  - Fast ramp (4ms/15step)");
  Serial.println("  - HTTP fast path + MQTT");
  Serial.println("  - Autopilot self-driving");
  Serial.println("  - Watchdog timer (8s)");
  Serial.println("  - Servo + Ultrasonic + Autopilot");
  Serial.println("======================================");

  // Watchdog timer — auto-reset if loop hangs for 15s
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 15000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_reconfigure(&wdt_config);
  esp_task_wdt_add(NULL);

  setupSensors();     // I2C + MCP23017 + 8x VL53L0X
  calibrateCliff();   // measure floor distance for cliff detection
  setupServo();       // needs its own 50Hz LEDC channel
  setupUltrasonic();
  setupMotors();      // 20kHz channels for motors
  setRadarPublisher(radarPublish);
  setAutoPublisher(autoStatusPublish);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
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

    // Start HTTP server
    setupHTTPServer();

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
  // Feed watchdog
  esp_task_wdt_reset();

  // Core tasks
  handleSerial();
  updateMotors();
  updateDemo();
  updateRadar();
  updateAutopilot();

  // HTTP server
  if (wifiConnected) {
    server.handleClient();
  }

  // Wi-Fi recovery
  checkWifiConnection();

  // MQTT
  if (MQTT_ENABLED) {
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
      // Reconnect every 5s
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
