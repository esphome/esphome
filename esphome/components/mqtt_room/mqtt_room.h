#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_rssi/ble_rssi_sensor.h"
#include "esphome/components/mqtt/mqtt_client.h"

namespace esphome {
namespace mqtt_room {
class MqttRoom : public Component {
 public:
  void dump_config() override;
  void set_topic(const std::string &topic);
  void add_tracker(ble_rssi::BLERSSISensor *sensor, const std::string &id, const std::string &name);

 protected:
  std::string mqtt_topic_;
  int tracker_count_ = 0;

  float calculateDistance(float rssi);
};
}  // namespace mqtt_room
}  // namespace esphome
