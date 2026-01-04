#include <WiFi.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "Android 2.4Ghz";
const char* password = "fudhail@:)";

// MQTT Broker settings
const char* mqtt_server = "broker.hivemq.com"; // Free public broker
const int mqtt_port = 1883;
const char* mqtt_topic = "water/monitor/sensor1";

// Sensor pins
#define TRIG_PIN 5
#define ECHO_PIN 18

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  
  // Setup ultrasonic sensor
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32-WaterMonitor-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected!");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

float getWaterLevel() {
  // Send ultrasonic pulse
  float distance = 30;
  
  // Convert to water level (assuming tank height is 100cm)
  float tankHeight = 100.0;
  float waterLevel = tankHeight - distance;
  
  if (waterLevel < 0) waterLevel = 0;
  if (waterLevel > tankHeight) waterLevel = tankHeight;
  
  return waterLevel;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Read sensor data
  float waterLevel = getWaterLevel();
  float waterPercentage = (waterLevel / 100.0) * 100.0;
  
  // Create JSON message
  String message = "{";
  message += "\"level\":" + String(waterLevel, 2) + ",";
  message += "\"percentage\":" + String(waterPercentage, 2) + ",";
  message += "\"timestamp\":" + String(millis());
  message += "}";
  
  // Publish to MQTT
  client.publish(mqtt_topic, message.c_str());
  
  Serial.println("Published: " + message);
  
  // Wait 5 seconds before next reading
  delay(5000);
}