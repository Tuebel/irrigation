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
// durations and intervals in milliseconds
const int BLINK_INTERVAL = 1 * TASK_SECOND;
const int MQTT_LOOP_INTERVAL = 50 * TASK_MILLISECOND;
const int RELAY_ON_DURATION = 3 * TASK_SECOND;
const int MOISTURE_INTERVAL = 9 * TASK_SECOND;
const int MOISTURE_ON_DURATION = 1 * TASK_SECOND;
const int POWER_ON_DURATION = 22 * TASK_SECOND;
const uint64_t DEEP_SLEEP_DURATION = 60 * 60e6;
// connection to home-assistant
WiFiClient wifi_client;
PubSubClient mqtt_client;
HassMqttDevice moisture, relay;
// task scheduling
Scheduler scheduler;
void blink();
Task blink_task(BLINK_INTERVAL, TASK_FOREVER, &blink, &scheduler);
// restart delayed in setup
void deep_sleep();
Task deep_sleep_task(TASK_IMMEDIATE, TASK_ONCE, &deep_sleep, &scheduler);
// allow mqtt callbacks
void loop_mqtt();
Task loop_mqtt_task(MQTT_LOOP_INTERVAL, TASK_FOREVER, &loop_mqtt, &scheduler);
// moisture sensor is more reliable when the power is turned on a few seconds
// before reading the adc
void moisture_power_on();
Task moisture_start_task(MOISTURE_INTERVAL, TASK_FOREVER, &moisture_power_on,
                         &scheduler);
void moisture_measure();
Task moisture_end_task(TASK_IMMEDIATE, TASK_ONCE, &moisture_measure,
                       &scheduler);
void relay_off();
Task relay_off_task(TASK_IMMEDIATE, TASK_ONCE, &relay_off, &scheduler);

void blink() { digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); }

bool wifi_connected() { return WiFi.status() == WL_CONNECTED; }

void deep_sleep() {
  relay_off();
  moisture.makeUnavailable();
  relay.makeUnavailable();
  loop_mqtt();
  Serial.println("Enter deep sleep");
  ESP.deepSleep(DEEP_SLEEP_DURATION);
}

void loop_mqtt() { mqtt_client.loop(); }

void relay_off() {
  digitalWrite(RELAY_PIN, LOW);
  // after turning off so delays are avoided
  Serial.println("Relay OFF");
  relay.publishState("OFF");
}

void relay_on() {
  // before turning on so delays are avoided
  relay.publishState("ON");
  Serial.println("Relay ON");
  digitalWrite(RELAY_PIN, HIGH);
  // for safety auto turn off after delay
  relay_off_task.restartDelayed(RELAY_ON_DURATION);
}

void moisture_power_on() {
  digitalWrite(MOISTURE_POWER_PIN, HIGH);
  Serial.println("Moisture power ON");
  // measurement in 1sec
  moisture_end_task.restartDelayed(MOISTURE_ON_DURATION);
}

void moisture_measure() {
  // measure and publish
  double moisture_volt = 3.3 * analogRead(ADC_PIN) / 1024;
  char moisture_payload[4];
  dtostrf(moisture_volt, 3, 2, moisture_payload);
  digitalWrite(MOISTURE_POWER_PIN, LOW);
  Serial.println("Moisture power OFF");
  // publish after power off
  moisture.publishState(moisture_payload);
  Serial.print("Moisture ");
  Serial.println(moisture_volt);
  // automatic irrigation
  if (moisture_volt < 1.0) {
    relay_on();
  }
}

void setup_connection() {
  // Connect WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifi_password);
  // Try to connect for 10 seconds
  for (int i = 0; !wifi_connected() && i < 10; i++) {
    delay(500);
    Serial.println(".");
  }
  if (!wifi_connected()) {
    Serial.println("Failed to connect to WiFi");
  } else {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    // Setup MQTT client
    mqtt_client.setClient(wifi_client);
    mqtt_client.setServer(mqtt_server, mqtt_port);
  }
}

void relay_callback(char* msg) {
  // handle message arrived
  Serial.print("Relay received command ");
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
  if (wifi_connected()) {
    Serial.println("Connecting moisture sensor to home-assistant");
    if (moisture.connect(mqtt_client, mqtt_user, mqtt_password)) {
      moisture.makeAvailable();
    }
    Serial.println("Connecting relay to home-assistant");
    if (relay.connect(mqtt_client, mqtt_user, mqtt_password)) {
      relay.makeAvailable();
      relay.subscribeCommands(relay_callback);
    }
  }
  // run tasks
  blink_task.enable();
  loop_mqtt_task.enable();
  moisture_start_task.enable();
  deep_sleep_task.restartDelayed(POWER_ON_DURATION);
  Serial.println("Device ready");
}

void loop() {
  // leave flow control to the scheduler
  scheduler.execute();
}
