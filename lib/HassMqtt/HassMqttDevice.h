#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <functional>

class HassMqttDevice {
 public:
  /** Size of the topic char arrays */
  const static int MAX_TOPIC_SIZE = 150;
  /** Size of the mqtt device configuration variable char arrayd */
  const static int MAX_CONFIG_VARIABLE_SIZE = 50;
  /** Number of tries to reconnect to the mqtt server */
  const static int MAX_RECONNECT_TRIES = 5;
  /** callback for commands
   * \param payload of the MQTT message */
  using CommandCallback = std::function<void(char*)>;

 private:
  // mqtt communication with home-assistant
  PubSubClient* client;
  CommandCallback cmd_callback;
  char username[MAX_CONFIG_VARIABLE_SIZE];
  char password[MAX_CONFIG_VARIABLE_SIZE];
  char component[MAX_CONFIG_VARIABLE_SIZE];
  char icon[MAX_CONFIG_VARIABLE_SIZE];
  bool is_commandable;
  char name[MAX_CONFIG_VARIABLE_SIZE];
  char node_id[MAX_CONFIG_VARIABLE_SIZE];
  char object_id[MAX_CONFIG_VARIABLE_SIZE];
  char unit_of_measurement[MAX_CONFIG_VARIABLE_SIZE];

  /** Creates the availability topic from the base topic
   * \param availability_topic has MAX_TOPIC_SIZE, is filled with availability
   * topic
   * \returns false, if the max_topic_id is exceeded. true on success */
  bool availabilityTopic(char* availability_topic);

  /** Creates the base topic from the component, node_id and object_id
   * \param base_topic has MAX_TOPIC_SIZE, is filled with base topic
   * \returns false, if the max_topic_id is exceeded. true on success */
  bool baseTopic(char* base_topic);

  /** Creates the command topic from the base topic
   * \param command_topic has MAX_TOPIC_SIZE, is filled with command topic
   * \returns false, if the max_topic_id is exceeded. true on success */
  bool commandTopic(char* command_topic);

  /** Creates the config topic from the base topic
   * \param config_topic has MAX_TOPIC_SIZE, is filled with config topic
   * \returns false, if the max_topic_id is exceeded. true on success */
  bool configTopic(char* config_topic);

  /** Creates the state topic from the base topic
   * \param state_topic has MAX_TOPIC_SIZE, is filled with state topic
   * \returns false, if the max_topic_id is exceeded. true on success */
  bool stateTopic(char* state_topic);

  /** Gets called when new mqtt messages arrive */
  void msg_callback(char* topic, byte* payload, unsigned int length);

  /** Tries to reconnect to the mqtt server only if neccessary.
   * \returns true if connected */
  bool reconnect();

 public:
  /** Connects this device to home-assistant server. It sends the configuration
   * for auto-discovery.
   * \param client preconfigured MQTT client (host, port)
   * \param username for the MQTT server
   * \param password for the MQTT server
   */
  bool connect(PubSubClient& client, const char* username,
               const char* password);

  /** Notify home-assistant that this device is available */
  void makeAvailable();
  
  /** Notify home-assistant that this device goes unavailable */
  void makeUnavailable();

  /** Removes the auto-discovered device from home-assistant. */
  void remove();

  /** Publish the given payload to the state topic
   * \param state the state encoded as string */
  bool publishState(const char* state);

  /** Subscribes the command topic.
   * \param callback will be called when a command arrives */
  bool subscribeCommands(CommandCallback callback);

  /** One of the supported MQTT components, eg. binary_sensor. */
  void setComponent(const char* component);

  /** Is this an actor? */
  void setIsCommandable(bool is_commandable);

  /** Configure the displayed icon (https://cdn.materialdesignicons.com/3.5.95/)
   */
  void setIcon(const char* icon);

  /** Displayed name of this device. */
  void setName(const char* name);

  /** ID of the node providing the topic, this is not used by Home Assistant but
   * may be used to structure the MQTT topic. */
  void setNodeId(const char* node_id);

  /** The ID of the device. This is only to allow for separate topics for each
   * device and is not used for the entity_id. */
  void setObjectId(const char* object_id);

  /** Defines the units of measurement of the sensor, if any. */
  void setUnitOfMeasurement(const char* unit_of_measurement);
};
