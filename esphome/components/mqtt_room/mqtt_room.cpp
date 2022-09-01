#include "mqtt_room.h"

#include <cmath>

#ifdef USE_ESP32

namespace esphome {
namespace mqtt_room {
static const char *const TAG = "mqtt_room";

void MqttRoom::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Room:");
  ESP_LOGCONFIG(TAG, "  MQTT topic: '%s'", this->mqtt_topic_.c_str());
  ESP_LOGCONFIG(TAG, "  Number of trackers: %i", this->tracker_count_);
}

void MqttRoom::set_topic(const std::string &topic) { this->mqtt_topic_ = topic; }

void MqttRoom::add_tracker(ble_rssi::BLERSSISensor *sensor, const std::string &id, const std::string &name) {
  this->tracker_count_++;
  sensor->add_on_state_callback([this, id, name](float rssi) {
    float distance = this->calculate_distance_(rssi);
    ESP_LOGD(TAG, "Got distance of '%0.1fm' for '%s'", distance, id.c_str());

    mqtt::global_mqtt_client->publish_json(this->mqtt_topic_, [=](ArduinoJson::JsonObject root) -> void {
      root["distance"] = distance;
      root["id"] = id;
      root["name"] = name;
    });
  });
}

float MqttRoom::calculate_distance_(float rssi) {
  const float ratio = rssi * 1.0 / -72;
  float distance;

  if (ratio < 1.0) {
    distance = pow(ratio, 10);
  } else {
    distance = (0.89976) * pow(ratio, 7.7095) + 0.111;
  }

  return std::round(distance * 100) / 100;
}

}  // namespace mqtt_room
}  // namespace esphome

#endif
