#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <HassMqttDevice.h>
#include <PubSubClient.h>
#include <TaskScheduler.h>
#include "device_config.h"

/* The device_config.h defines the following secret constants:
const char ssid[] = "My_Wifi_Router_SSID";
const char wifi_password[] = "My_Wifi_Password";
const char mqtt_server[] = "My_MQTT_Host";
const uint16_t mqtt_port = 1883;
const char mqtt_user[] = "my_mqtt_device";
const char mqtt_password[] = "My_MQTT_Password";
*/

// hardware access
const int RELAY_PIN = D1;
const int MOISTURE_POWER_PIN = D2;
const int ADC_PIN = A0;
// connection to home-assistant
WiFiClient wifi_client;
PubSubClient mqtt_client;
HassMqttDevice moisture, relay;
// task scheduling
Scheduler scheduler;
void blink();
Task blink_task(1e3, TASK_FOREVER, &blink, &scheduler);
// restart delayed in setup
void deep_sleep();
Task deep_sleep_task(0, 1, &deep_sleep, &scheduler);
// allow mqtt callbacks
void loop_mqtt();
Task loop_mqtt_task(100, TASK_FOREVER, &loop_mqtt, &scheduler);
// moisture sensor is more reliable when the power is turned on a few seconds
// before reading the adc
void moisture_power_on();
void moisture_measure();
Task moisture_task(1e3, TASK_FOREVER, &moisture_power_on, &scheduler);
void relay_off();
// run once, restart delayed after every relay on
Task relay_off_task(1e3, 1, &relay_off, &scheduler);

void blink() { digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); }

void deep_sleep() {
  relay.makeUnavailable();
  loop_mqtt();
  Serial.println("Enter deep sleep");
  ESP.deepSleep(900e6);
}

void loop_mqtt() { mqtt_client.loop(); }

void moisture_power_on() {
  digitalWrite(MOISTURE_POWER_PIN, HIGH);
  // measurement in 1sec
  moisture_task.setCallback(&moisture_measure);
  moisture_task.setInterval(1e3);
}
void moisture_measure() {
  double moisture_volt = 3.3 * analogRead(ADC_PIN) / 1024;
  char moisture_payload[4];
  dtostrf(moisture_volt, 3, 2, moisture_payload);
  moisture.publishState(moisture_payload);
  digitalWrite(MOISTURE_POWER_PIN, LOW);
  // restart measurement in 10secs
  moisture_task.setCallback(&moisture_power_on);
  moisture_task.setInterval(10e3);
}

void relay_off() {
  digitalWrite(RELAY_PIN, LOW);
  relay.publishState("OFF");
  loop_mqtt();
}

void relay_on() {
  digitalWrite(RELAY_PIN, HIGH);
  relay.publishState("ON");
  // for safety auto turn off after delay
  relay_off_task.restartDelayed(2000);
}

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

void relay_callback(char* msg) {
  // handle message arrived
  Serial.print("relay received command ");
  Serial.println(msg);
  if (strcmp(msg, "ON") == 0) {
    relay_on();
  } else {
    // LOW is safe
    relay_off();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  // wait for the serial
  delay(500);
  // setup moisture sensor
  moisture.setComponent("sensor");
  moisture.setIcon("mdi:water-percent");
  moisture.setIsCommandable(false);
  moisture.setName("Feuchtigkeit Weihnachtsstern");
  moisture.setNodeId("esp8266_christmas_star");
  moisture.setObjectId("moisture_christmas_star");
  moisture.setUnitOfMeasurement("Volt");
  // setup relay
  relay.setComponent("switch");
  relay.setIcon("mdi:water-pump");
  relay.setIsCommandable(true);
  relay.setName("Pumpschalter");
  relay.setNodeId("esp8266_christmas_star");
  relay.setObjectId("relay_christmas_star");
  // config pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ADC_PIN, INPUT);
  pinMode(MOISTURE_POWER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  // connect
  setup_connection();
  while (!moisture.connect(mqtt_client, mqtt_user, mqtt_password)) {
    Serial.println("connecting moisture to home-assistant in 5 seconds");
    delay(5000);
  }
  moisture.makeAvailable();
  while (!relay.connect(mqtt_client, mqtt_user, mqtt_password)) {
    Serial.println("connecting moisture to home-assistant in 5 seconds");
    delay(5000);
  }
  relay.makeAvailable();
  relay.subscribeCommands(relay_callback);
  // run tasks
  blink_task.enable();
  // deep sleep in 30s
  deep_sleep_task.restartDelayed(15e3);
  loop_mqtt_task.enable();
  moisture_task.enable();
  Serial.println("Device ready");
}

void loop() {
  // leave flow control to the scheduler
  scheduler.execute();
}
