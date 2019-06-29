# Goals
* Integration into [home-assistant](https://www.home-assistant.io/) via MQTT. [ESPHome](https://esphome.io/) is really cool but I just love coding.
Also as the ESP8266 currently serves as server in ESPHome, it needs to be connected at least 60s to be discovered after deep sleep.
* Publish the moisture of the soil to the home-assistant instance.
* Automatically and manually switch a relay to moisturize the soil.
* It is planned to publish the home-assistant MQTT device library as separate git.

# Used Libraries
Thanks to all the authors for the greate libraries:
* [ArduinoJson](https://arduinojson.org/) for serialization
* [PubSubClient](https://pubsubclient.knolleary.net/) for mqtt communication
* [TaskScheduler](https://playground.arduino.cc/Code/TaskScheduler/) for cooperative multi-tasking
