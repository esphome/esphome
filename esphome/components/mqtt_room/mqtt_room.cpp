#include "mqtt_room.h"

#include <cmath>

#ifdef USE_ESP32

namespace esphome {
namespace mqtt_room {
static const char *const TAG = "mqtt_room";

void MqttRoom::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Room:");
  ESP_LOGCONFIG(TAG, "  MQTT topic: '%s'", this->mqtt_topic_.c_str());
}

void MqttRoom::set_topic(const std::string &topic) { this->mqtt_topic_ = topic; }

void MqttRoom::add_tracker(MqttRoomTracker *tracker) { tracker->set_mqtt_room(this); }

void MqttRoom::send_tracker_update(const std::string id, const std::string name, float distance) {
  distance = std::round(distance * 100) / 100;
  ESP_LOGD(TAG, "'%s': Sending state %.2f m with 2 decimals of accuracy", id.c_str(), distance);

  mqtt::global_mqtt_client->publish_json(this->mqtt_topic_, [=](ArduinoJson::JsonObject root) -> void {
    root["distance"] = distance;
    root["id"] = id;
    root["name"] = name;
  });
}

}  // namespace mqtt_room
}  // namespace esphome

#endif
