#include "mqtt_room.h"

namespace esphome {
namespace mqtt_room {
static const char *const TAG = "mqtt_room";

void MqttRoom::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Room:");
  ESP_LOGCONFIG(TAG, "  Room name: '%s'", this->room_.c_str());
}

void MqttRoom::set_room(const std::string &room) { this->room_ = room; }

void MqttRoom::add_tracker(ble_rssi::BLERSSISensor *sensor, const std::string &id, const std::string &name) {
  sensor->add_on_state_callback([this, id, name](float rssi) {
    float distance = this->calculateDistance(rssi);
    ESP_LOGD(TAG, "Got distance of '%0.1fm' for '%s'", distance, id.c_str());

    mqtt::global_mqtt_client->publish_json("esphome/rooms/" + this->room_, [=](ArduinoJson::JsonObject root) -> void {
      root["distance"] = distance;
      root["id"] = id;
      root["name"] = name;
    });
  });
}

float MqttRoom::calculateDistance(float rssi) {
  const float ratio = rssi * 1.0 / -72;
  float distance;

  if (ratio < 1.0) {
    distance = pow(ratio, 10);
  } else {
    distance = (0.89976) * pow(ratio, 7.7095) + 0.111;
  }

  return round(distance * 100) / 100;
}

}  // namespace mqtt_room
}  // namespace esphome
