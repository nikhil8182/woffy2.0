/*
 * Woffy Config - MQTT Only
 */

#define WOFFY_SSID   "Onwords"
#define WOFFY_PASS   "nikhil8182"

#define WIFI_CONNECT_TIMEOUT 10000

#ifndef DEFAULT_SPEED
#define DEFAULT_SPEED 180
#endif

// ========== MQTT Settings ==========
#define MQTT_ENABLED       true
#define MQTT_BROKER       "mqtt.onwords.in"
#define MQTT_PORT         1883
#define MQTT_CLIENT_ID    "woffy_esp32"
#define MQTT_USERNAME    "Nikhil"
#define MQTT_PASSWORD    "Nikhil8182"
#define MQTT_TOPIC_CMD    "woffy/command"
#define MQTT_TOPIC_STATUS "woffy/status"
