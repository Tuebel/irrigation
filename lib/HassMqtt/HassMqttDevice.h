#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <functional>

class HassMqttDevice {
 public:
  const static int max_topic_size = 150, max_name_size = 50,
                   max_node_id_size = 50, max_object_id_size = 50,
                   max_component_size = 30, max_reconnect_tries = 5,
                   max_username_size = 50, max_password_size = 50;
  // callback for commands, has a char* payload
  using CommandCallback = std::function<void(char*)>;

 private:
  // mqtt communication with home-assistant
  PubSubClient* client;
  CommandCallback cmd_callback;
  char username[max_username_size];
  char password[max_password_size];
  // e.g. binary_sensor
  char component[max_component_size];
  // e.g. a switch is commandable, a sensor not
  bool is_commandable;
  // display name
  char name[max_name_size];
  // e.g. esp8266_livingroom
  char node_id[max_node_id_size];
  // e.g. light_switch
  char object_id[max_object_id_size];

  // creates the base topic, returns false, if the max_topic_id is exceeded
  bool baseTopic(char* base_topic);
  bool commandTopic(char* command_topic);
  bool configTopic(char* config_topic);
  bool stateTopic(char* state_topic);
  // gets called when new mqtt messages arrive
  void msg_callback(char* topic, byte* payload, unsigned int length);
  // tries to reconnect if neccessary. Returns true if connected
  bool reconnect();

 public:
  // connects this device to home-assistant with auto discovery via the given
  // mqtt client
  bool connect(PubSubClient& client, const char* username,
               const char* password);
  // removes the auto-discovered device from home-assistant
  void remove();

  // publish the given payload to the state topic
  bool publishState(const char* state);
  // callback for new commands
  bool subscribeCommands(CommandCallback callback);

  // type of component, e.g.: "sensor" or "switch"
  void setComponent(const char* component);
  // any actor or state settable?
  void setIsCommandable(bool is_commandable);
  void setName(const char* name);
  void setNodeId(const char* node_id);
  void setObjectId(const char* object_id);
};
