#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// config
const char* ssid = "none";
const char* password = "none";
const char* mqtt_broker = "broker.hivemq.com"; 

// pins
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define FAN_PIN 18
#define PUMP_PIN 19

// thresholds
const float temp_warn = 33.0;
const float temp_crit = 37.0;

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient espClient;
PubSubClient client(espClient);

// esp32 pwm settings
const int pwmCh = 0;
const int pwmFreq = 5000;
const int pwmRes = 8;

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ECON-Node-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      // connected
    } else {
      delay(5000); // retry
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  
  ledcSetup(pwmCh, pwmFreq, pwmRes);
  ledcAttachPin(FAN_PIN, pwmCh);
  ledcWrite(pwmCh, 0);

  setup_wifi();
  client.setServer(mqtt_broker, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  delay(5000);

  float hum = dht.readHumidity();
  float temp = dht.readTemperature();

  if (isnan(hum) || isnan(temp)) {
    Serial.println("dht error");
    return;
  }

  int fan_pct = 0;
  String current_mode = "";
  bool pump_on = false;

  // control logic
  if (temp < temp_warn) {
    current_mode = "ECO";
    fan_pct = 30;
    pump_on = false;
  } 
  else if (temp >= temp_warn && temp < temp_crit) {
    current_mode = "NORMAL";
    fan_pct = 60;
    pump_on = true;
  } 
  else {
    current_mode = "MAX";
    fan_pct = 100;
    pump_on = true;
  }

  // apply hardware states
  int pwm_val = map(fan_pct, 0, 100, 0, 255);
  ledcWrite(pwmCh, pwm_val);
  digitalWrite(PUMP_PIN, pump_on ? HIGH : LOW);

  // build json payload
  String payload = "{";
  payload += "\"id\":\"ECON-TDK-01\",";
  payload += "\"t\":" + String(temp, 1) + ",";
  payload += "\"h\":" + String(hum, 1) + ",";
  payload += "\"mode\":\"" + current_mode + "\",";
  payload += "\"fan\":" + String(fan_pct) + ",";
  payload += "\"pump\":" + String(pump_on ? 1 : 0);
  payload += "}";

  Serial.println(payload);
  client.publish("econ/tdk/telemetry", payload.c_str());
}
