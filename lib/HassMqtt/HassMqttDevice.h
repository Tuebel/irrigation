#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <functional>

class HassMqttDevice {
 public:
  const static int MAX_TOPIC_SIZE = 150, MAX_NAME_SIZE = 50,
                   MAX_NODE_ID_SIZE = 50, MAX_OBJECT_ID_SIZE = 50,
                   MAX_COMPONENT_SIZE = 30, MAX_RECONNECT_TRIES = 5,
                   MAX_USERNAME_SIZE = 50, MAX_PASSWORD_SIZE = 50;
  // callback for commands, has a char* payload
  using CommandCallback = std::function<void(char*)>;

 private:
  // mqtt communication with home-assistant
  PubSubClient* client;
  CommandCallback cmd_callback;
  char username[MAX_USERNAME_SIZE];
  char password[MAX_PASSWORD_SIZE];
  // e.g. binary_sensor
  char component[MAX_COMPONENT_SIZE];
  // e.g. a switch is commandable, a sensor not
  bool is_commandable;
  // display name
  char name[MAX_NAME_SIZE];
  // e.g. esp8266_livingroom
  char node_id[MAX_NODE_ID_SIZE];
  // e.g. light_switch
  char object_id[MAX_OBJECT_ID_SIZE];

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
