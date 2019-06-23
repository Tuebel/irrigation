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

bool HassMqttDevice::baseTopic(char* base_topic) {
  strcpy(base_topic, discovery_prefix);
  bool valid = true;
  valid &= strcat_safe(base_topic, "/", max_topic_size);
  valid &= strcat_safe(base_topic, component, max_topic_size);
  valid &= strcat_safe(base_topic, "/", max_topic_size);
  valid &= strcat_safe(base_topic, node_id, max_topic_size);
  valid &= strcat_safe(base_topic, "/", max_topic_size);
  valid &= strcat_safe(base_topic, object_id, max_topic_size);
  return valid;
}
bool HassMqttDevice::configTopic(char* config_topic) {
  bool valid = true;
  valid &= baseTopic(config_topic);
  valid &= strcat_safe(config_topic, "/config", max_topic_size);
  return valid;
}
bool HassMqttDevice::commandTopic(char* command_topic) {
  if (is_commandable) {
    bool valid = true;
    valid &= baseTopic(command_topic);
    valid &= strcat_safe(command_topic, "/cmd", max_topic_size);
    return valid;
  } else {
    strcpy(command_topic, "");
    return true;
  }
}
bool HassMqttDevice::stateTopic(char* state_topic) {
  bool valid = true;
  valid &= baseTopic(state_topic);
  valid &= strcat_safe(state_topic, "/stat", max_topic_size);
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
  Serial.print(" on topic: ");
  Serial.print(topic);
  Serial.print(" with length ");
  Serial.println(length);
  char cmd_topic[max_topic_size];
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
  char client_id[max_node_id_size + max_object_id_size + 2];
  strcpy(client_id, node_id);
  strcat(client_id, "/");
  strcat(client_id, object_id);
  for (int i = 0; i < max_reconnect_tries; i++) {
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
  const size_t capacity = JSON_OBJECT_SIZE(5);
  DynamicJsonDocument doc(capacity);
  bool valid = true;
  doc["name"] = name;
  char cmd_topic[max_topic_size];
  valid &= commandTopic(cmd_topic);
  doc["cmd_t"] = (const char*)cmd_topic;
  char stat_topic[max_topic_size];
  valid &= stateTopic(stat_topic);
  doc["stat_t"] = (const char*)stat_topic;
  if (!valid) {
    Serial.print("could not connect to home-assistant: ");
    Serial.println("name, cmd_topic or stat_topic invalid (max. 150chars)");
    return false;
  }
  // publish to config topic
  char config_topic[max_topic_size];
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

void HassMqttDevice::remove() {
  char config_topic[max_topic_size];
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
  char state_topic[max_topic_size];
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
  char cmd_topic[max_topic_size];
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
  strncpy(this->component, component, max_component_size);
}

void HassMqttDevice::setIsCommandable(bool is_commandable) {
  this->is_commandable = is_commandable;
}

void HassMqttDevice::setName(const char* name) {
  strncpy(this->name, name, max_name_size);
}

void HassMqttDevice::setNodeId(const char* node_id) {
  strncpy(this->node_id, node_id, max_node_id_size);
}

void HassMqttDevice::setObjectId(const char* object_id) {
  strncpy(this->object_id, object_id, max_object_id_size);
}
