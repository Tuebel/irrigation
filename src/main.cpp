#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <HassMqttDevice.h>
#include <PubSubClient.h>
#include "device_config.h"

// hardware access
unsigned long previous_millis = 0;
const long interval = 1000;
const int relayPin = D1;
WiFiClient wifi_client;
PubSubClient mqtt_client;
HassMqttDevice moisture, relay;

void setup_connection() {
  // Connect WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  // Setup MQTT client
  mqtt_client.setClient(wifi_client);
  mqtt_client.setServer(mqtt_server, mqtt_port);
}

void relay_switch_callback(char* data, uint16_t len) {
  // handle message arrived
  Serial.print(data);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  // wait for the serial
  delay(500);
  // setup moisture sensor
  moisture.setComponent("sensor");
  moisture.setIsCommandable(false);
  moisture.setName("Feuchtigkeit Weihnachtsstern");
  moisture.setNodeId("esp8266_christmas_star");
  moisture.setObjectId("moisture_christmas_star");
  // setup relay
  relay.setComponent("switch");
  relay.setIsCommandable(true);
  relay.setName("Pumpschalter");
  relay.setNodeId("esp8266_christmas_star");
  relay.setObjectId("relay_christmas_star");
  // connect
  setup_connection();
  while (!moisture.connect(mqtt_client, mqtt_user, mqtt_password)) {
    Serial.println("connecting moisture to home-assistant in 5 seconds");
    delay(5000);
  }
  while (!relay.connect(mqtt_client, mqtt_user, mqtt_password)) {
    Serial.println("connecting moisture to home-assistant in 5 seconds");
    delay(5000);
  }
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(A0, INPUT);
  pinMode(relayPin, OUTPUT);
  Serial.println("Device ready");
}

double voltage33(int value) { return value * 3.3 / 1024; }

void loop() {
  mqtt_client.loop();
  // toggle internal LED
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  // read the moisture voltage
  double moisture_volt = voltage33(analogRead(A0));
  char moisture_payload[4];
  dtostrf(moisture_volt, 3, 2, moisture_payload);
  moisture.publishState(moisture_payload);
  delay(1000);
  // // turn on relay with high
  // digitalWrite(relayPin, HIGH);
  // delay(1000);
  // digitalWrite(LED_BUILTIN, HIGH);
  // digitalWrite(relayPin, LOW);
  // delay(2000);
}
