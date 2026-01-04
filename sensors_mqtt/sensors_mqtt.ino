#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// WiFi credentials
const char* ssid = "Android 2.4Ghz";
const char* password = "fudhail@:)";

// MQTT Broker settings
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "water/quality/sensor1";

// Sensor pins
#define PH_PIN 34           // Analog pin for pH sensor
#define TDS_PIN 35          // Analog pin for TDS sensor
#define TURBIDITY_PIN 32    // Analog pin for turbidity sensor
#define TEMP_PIN 26          // Digital pin for DS18B20 temperature sensor
#define FLOW_PIN 5          // Digital pin for flow sensor

// Flow sensor variables
volatile int flowPulseCount = 0;
float flowRate = 0.0;
float flowCalibrationFactor = 4.5; // Pulses per liter (adjust based on your sensor)

WiFiClient espClient;
PubSubClient client(espClient);

// Temperature sensor setup
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

void IRAM_ATTR flowPulseCounter() {
  flowPulseCount++;
}

void setup() {
  Serial.begin(115200);
  
  // Setup analog pins
  pinMode(PH_PIN, INPUT);
  pinMode(TDS_PIN, INPUT);
  pinMode(TURBIDITY_PIN, INPUT);
  
  // Setup flow sensor
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowPulseCounter, FALLING);
  
  // Initialize temperature sensor
  tempSensor.begin();
  
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
    String clientId = "ESP32-WaterQuality-";
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

float readPH() {
  int sensorValue = analogRead(PH_PIN);
  float voltage = sensorValue * (3.3 / 4095.0);
  // PH calculation (calibrate based on your sensor)
  // Typical: pH = 7.0 at 2.5V (neutral)
  float ph = 7.0 + ((2.5 - voltage) / 0.18);
  
  // Constrain to valid pH range
  if (ph < 0) ph = 0;
  if (ph > 14) ph = 14;
  
  return ph;
}

float readTDS() {
  int sensorValue = analogRead(TDS_PIN);
  float voltage = sensorValue * (3.3 / 4095.0);
  
  // TDS calculation (calibrate based on your sensor)
  // Assuming compensation temperature of 25°C
  float tdsValue = (133.42 * voltage * voltage * voltage 
                    - 255.86 * voltage * voltage 
                    + 857.39 * voltage) * 0.5;
  
  return tdsValue;
}

float readTurbidity() {
  int sensorValue = analogRead(TURBIDITY_PIN);
  float voltage = sensorValue * (3.3 / 4095.0);
  
  // Turbidity calculation (NTU)
  // Lower voltage = higher turbidity
  // float turbidity = -1120.4 * voltage * voltage + 5742.3 * voltage - 4352.9;
  float turbidity = voltage;
  
  // Constrain to reasonable range
  if (turbidity < 0) turbidity = 0;
  if (turbidity > 3000) turbidity = 3000;
  
  return turbidity;
}

float readTemperature() {
  tempSensor.requestTemperatures();
  float temp = tempSensor.getTempCByIndex(0);
  
  // Check for sensor error
  if (temp == DEVICE_DISCONNECTED_C) {
    return 25.0; // Return default if sensor disconnected
  }
  
  return temp;
}

float calculateFlowRate() {
  // Calculate flow rate based on pulse count
  // This runs every second in the loop
  flowRate = (flowPulseCount / flowCalibrationFactor); // Liters per minute
  flowPulseCount = 0; // Reset counter
  
  return flowRate;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Read all sensor data
  // float ph = readPH();
  // float tds = readTDS();
  // float turbidity = readTurbidity();
  // float temperature = readTemperature();
  // float flow = calculateFlowRate();

  float ph = 0.0;
  float tds = 0.0;
  float turbidity = readTurbidity();
  float temperature = readTemperature();
  float flow = 0.0;
  
  // Create JSON message
  String message = "{";
  message += "\"ph\":" + String(ph, 2) + ",";
  message += "\"tds\":" + String(tds, 2) + ",";
  message += "\"turbidity\":" + String(turbidity, 2) + ",";
  message += "\"temperature\":" + String(temperature, 2) + ",";
  message += "\"flowrate\":" + String(flow, 2) + ",";
  message += "\"timestamp\":" + String(millis());
  message += "}";
  
  // Publish to MQTT
  client.publish(mqtt_topic, message.c_str());
  
  Serial.println("Published: " + message);
  
  // Wait before next reading
  delay(1000); // 1 second for flow rate calculation
}