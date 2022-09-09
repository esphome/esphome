#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

namespace esphome {
namespace mqtt_room {
class MqttRoomTracker;

class MqttRoom : public Component {
 public:
  void dump_config() override;
  void set_topic(const std::string &topic);
  void add_tracker(MqttRoomTracker *tracker);
  void send_tracker_update(const std::string id, const std::string name, float distance);

 protected:
  std::string mqtt_topic_;
};

class MqttRoomTracker {
 public:
  void set_id(const std::string &id) { this->id_ = id; }
  void set_name(const std::string &name) { this->name_ = name; };
  void set_mqtt_room(MqttRoom *mqtt_room) { this->mqtt_room_ = mqtt_room; }
  std::string id() { return this->id_; }

 protected:
  std::string name_;
  std::string id_;
  MqttRoom *mqtt_room_;
};
}  // namespace mqtt_room
}  // namespace esphome

#endif
