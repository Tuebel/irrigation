#include "HassMqttDevice.h"
#include <ArduinoJson.h>

const char discovery_prefix[] = "homeassistant";

// Safe version of strcat. Concatenates only if enough space is left in dest.
// Returns true if characters have been copied;
bool strcat_safe(char* dest, const char* src, int dest_size) {
  // at least one char for null termination
  if (dest_size - strlen(dest) - strlen(src) > 0) {
    strcat(dest, src);
    return true;
  } else {
    return false;
  }
}

bool HassMqttDevice::availabilityTopic(char* availability_topic) {
  bool valid = true;
  valid &= baseTopic(availability_topic);
  valid &= strcat_safe(availability_topic, "/available", MAX_TOPIC_SIZE);
  return valid;
}

bool HassMqttDevice::baseTopic(char* base_topic) {
  strcpy(base_topic, discovery_prefix);
  bool valid = true;
  valid &= strcat_safe(base_topic, "/", MAX_TOPIC_SIZE);
  valid &= strcat_safe(base_topic, component, MAX_TOPIC_SIZE);
  valid &= strcat_safe(base_topic, "/", MAX_TOPIC_SIZE);
  valid &= strcat_safe(base_topic, node_id, MAX_TOPIC_SIZE);
  valid &= strcat_safe(base_topic, "/", MAX_TOPIC_SIZE);
  valid &= strcat_safe(base_topic, object_id, MAX_TOPIC_SIZE);
  return valid;
}
bool HassMqttDevice::configTopic(char* config_topic) {
  bool valid = true;
  valid &= baseTopic(config_topic);
  valid &= strcat_safe(config_topic, "/config", MAX_TOPIC_SIZE);
  return valid;
}
bool HassMqttDevice::commandTopic(char* command_topic) {
  bool valid = true;
  valid &= baseTopic(command_topic);
  valid &= strcat_safe(command_topic, "/cmd", MAX_TOPIC_SIZE);
  return valid;
}
bool HassMqttDevice::stateTopic(char* state_topic) {
  bool valid = true;
  valid &= baseTopic(state_topic);
  valid &= strcat_safe(state_topic, "/stat", MAX_TOPIC_SIZE);
  return valid;
}

void HassMqttDevice::msg_callback(char* topic, byte* payload,
                                  unsigned int length) {
  // convert to null terminated string
  char msg[length + 1];
  msg[0] = '\0';
  strncat(msg, (const char*)payload, length);
  // debug log
  Serial.print("reveived msg: ");
  Serial.print(msg);
  Serial.print(" topic: ");
  Serial.println(topic);
  char cmd_topic[MAX_TOPIC_SIZE];
  if (!commandTopic(cmd_topic)) {
    Serial.print("failed to call command callback: ");
    Serial.println("command topic too long");
    return;
  }
  if (strcmp(topic, cmd_topic) == 0) {
    if (cmd_callback) {
      cmd_callback(msg);
    } else {
      Serial.print("failed to call command callback: ");
      Serial.println("no cmd_callback configured");
    }
  }
}

bool HassMqttDevice::reconnect() {
  if (client->connected()) {
    return true;
  }
  char client_id[MAX_CONFIG_VARIABLE_SIZE + MAX_CONFIG_VARIABLE_SIZE + 2];
  strcpy(client_id, node_id);
  strcat(client_id, "/");
  strcat(client_id, object_id);
  for (int i = 0; i < MAX_RECONNECT_TRIES; i++) {
    if (!client->connect(client_id, username, password)) {
      Serial.print("failed to connect mqtt, rc=");
      Serial.print(client->state());
      Serial.print("retry in 5 seconds");
      delay(5000);
    } else {
      break;
    }
  }
  return client->connected();
}

bool HassMqttDevice::connect(PubSubClient& client, const char* username,
                             const char* password) {
  // copy the params
  this->client = &client;
  strcpy(this->username, username);
  strcpy(this->password, password);
  // connect to mqtt server;
  if (!reconnect()) {
    Serial.print("could not connect to home-assistant: ");
    Serial.println("failed to connect to MQTT server");
    return false;
  }
  // config payload
  const size_t capacity = JSON_OBJECT_SIZE(6);
  DynamicJsonDocument doc(capacity);
  bool valid = true;
  if (strlen(icon) > 0) {
    doc["icon"] = (const char*)icon;
  }
  if (strlen(name) > 0) {
    doc["name"] = (const char*)name;
  }
  if (strlen(unit_of_measurement) > 0) {
    doc["unit_of_measurement"] = (const char*)unit_of_measurement;
  }
  // topics
  char availability_topic[MAX_TOPIC_SIZE];
  valid &= availabilityTopic(availability_topic);
  doc["avty_t"] = (const char*)availability_topic;
  char cmd_topic[MAX_TOPIC_SIZE];
  if (is_commandable) {
    valid &= commandTopic(cmd_topic);
    doc["cmd_t"] = (const char*)cmd_topic;
  }
  char stat_topic[MAX_TOPIC_SIZE];
  valid &= stateTopic(stat_topic);
  doc["stat_t"] = (const char*)stat_topic;
  if (!valid) {
    Serial.print("could not connect to home-assistant: ");
    Serial.println("name, cmd_topic or stat_topic invalid (max. 150chars)");
    return false;
  }
  char config_topic[MAX_TOPIC_SIZE];
  valid &= configTopic(config_topic);
  if (!valid) {
    Serial.print("could not connect to home-assistant: ");
    Serial.println("config topic invalid (max. 150chars)");
    return false;
  }
  // publish via stream as the json gets large
  Serial.print("publish config: ");
  serializeJson(doc, Serial);
  Serial.print(" to topic ");
  Serial.println(config_topic);
  bool publish_res = client.beginPublish(config_topic, measureJson(doc), true);
  serializeJson(doc, client);
  publish_res &= client.endPublish();
  if (!publish_res) {
    Serial.print("failed to publish config rc ");
    Serial.println(client.state());
    return false;
  }
  return true;
}

void HassMqttDevice::makeAvailable() {
  char availability_topic[MAX_TOPIC_SIZE];
  if (!availabilityTopic(availability_topic)) {
    Serial.print("could not make device available: ");
    Serial.println("availability topic is not valid (max. 150chars)");
    return;
  }
  if (!client->publish(availability_topic, "online", true)) {
    Serial.print("could not make device available: ");
    Serial.print("either connection has been lost or message is too large");
    return;
  }
  Serial.print("Succesfully made device available");
}

void HassMqttDevice::makeUnavailable() {
  char availability_topic[MAX_TOPIC_SIZE];
  if (!availabilityTopic(availability_topic)) {
    Serial.print("could not make device unavailable: ");
    Serial.println("availability topic is not valid (max. 150chars)");
    return;
  }
  if (!client->publish(availability_topic, "offline", true)) {
    Serial.print("could not make device unavailable: ");
    Serial.print("either connection has been lost or message is too large");
    return;
  }
  Serial.print("Succesfully made device unavailable");
}

void HassMqttDevice::remove() {
  char config_topic[MAX_TOPIC_SIZE];
  if (!configTopic(config_topic)) {
    Serial.print("could not unregister device: ");
    Serial.println("config topic is not valid (max. 150chars)");
    return;
  }
  if (!client->publish(config_topic, "", true)) {
    Serial.print("could not unregister device: ");
    Serial.println("failed to publish the config");
    return;
  }
  Serial.println("successfully unregistered the device");
}

// publish the given payload to the state topic
bool HassMqttDevice::publishState(const char* state) {
  if (!reconnect()) {
    Serial.print("failed to publish state: ");
    Serial.println("MQTT client not connected");
    return false;
  }
  char state_topic[MAX_TOPIC_SIZE];
  if (!stateTopic(state_topic)) {
    Serial.println("failed to publish state: ");
    Serial.print("state topic is too long");
    return false;
  }
  if (!client->publish(state_topic, state, true)) {
    Serial.println("failed to publish state: ");
    Serial.print("either connection has been lost or message is too large");
    return false;
  }
  Serial.print("Sent new state: ");
  Serial.println(state);
  return true;
}
// callback for new commands
bool HassMqttDevice::subscribeCommands(CommandCallback callback) {
  // if commandable subscribe!
  if (!is_commandable) {
    Serial.print("failed to subsrcibe the command topic: ");
    Serial.println("device is not commandable");
    return false;
  }
  client->setCallback(std::bind(&HassMqttDevice::msg_callback, this,
                                std::placeholders::_1, std::placeholders::_2,
                                std::placeholders::_3));
  char cmd_topic[MAX_TOPIC_SIZE];
  if (!commandTopic(cmd_topic)) {
    Serial.print("failed to subsrcibe the command topic: ");
    Serial.println("command topic too long");
    return false;
  }
  if (!client->subscribe(cmd_topic, 1)) {
    Serial.print("failed to subsrcibe the command topic:");
    Serial.println("either connection lost, or message too large");
  }
  Serial.println("subscribed command topic");
  cmd_callback = callback;
  return true;
}

void HassMqttDevice::setComponent(const char* component) {
  strncpy(this->component, component, MAX_CONFIG_VARIABLE_SIZE);
}

void HassMqttDevice::setIcon(const char* icon) {
  strncpy(this->icon, icon, MAX_CONFIG_VARIABLE_SIZE);
}

void HassMqttDevice::setIsCommandable(bool is_commandable) {
  this->is_commandable = is_commandable;
}

void HassMqttDevice::setName(const char* name) {
  strncpy(this->name, name, MAX_CONFIG_VARIABLE_SIZE);
}

void HassMqttDevice::setNodeId(const char* node_id) {
  strncpy(this->node_id, node_id, MAX_CONFIG_VARIABLE_SIZE);
}

void HassMqttDevice::setObjectId(const char* object_id) {
  strncpy(this->object_id, object_id, MAX_CONFIG_VARIABLE_SIZE);
}

void HassMqttDevice::setUnitOfMeasurement(const char* unit_of_measurement) {
  strncpy(this->unit_of_measurement, unit_of_measurement,
          MAX_CONFIG_VARIABLE_SIZE);
}
