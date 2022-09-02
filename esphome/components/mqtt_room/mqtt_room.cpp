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

void MqttRoom::add_tracker(ble_rssi::BLERSSISensor *sensor, const std::string &name) {
  this->tracker_count_++;
  sensor->add_on_state_callback([this, sensor, name](float rssi) {
    float distance = this->calculate_distance_(rssi);
    ESP_LOGD(TAG, "'%s': Sending state %.2f m with 2 decimals of accuracy", sensor->get_object_id().c_str(), distance);

    mqtt::global_mqtt_client->publish_json(this->mqtt_topic_, [=](ArduinoJson::JsonObject root) -> void {
      root["distance"] = distance;
      root["id"] = sensor->get_object_id();
      root["name"] = name;
    });
  });
}

void MqttRoom::add_tracker(ble_rssi::BLERSSISensor *sensor) { this->add_tracker(sensor, sensor->get_name()); }

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
