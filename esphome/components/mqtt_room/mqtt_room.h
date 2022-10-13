#pragma once

#include <list>
#include "esphome/core/component.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"

#ifdef USE_ESP32

namespace esphome {
namespace mqtt_room {

class MqttRoom;

class MqttRoomTracker : public Component {
 public:
  void set_device_id(const std::string &device_id) { this->device_id_ = device_id; }
  void set_name(const std::string &name) { this->name_ = name; }
  void set_signal_power(int signal_power) { this->signal_power_ = signal_power; }
  void set_mqtt_room(MqttRoom *mqtt_room) { this->mqtt_room_ = mqtt_room; }
  void set_rssi_sensor(sensor::Sensor *rssi_sensor);
  void set_distance_sensor(sensor::Sensor *distance_sensor);

  void dump_config() override;

  std::string get_device_id() { return this->device_id_; }
  void update_rssi_sensor(int rssi, int signal_power);
  void update_distance_sensor(int rssi);

 protected:
  std::string device_id_;
  std::string name_;
  int signal_power_;

  sensor::Sensor *rssi_sensor_;
  sensor::Sensor *distance_sensor_;

  MqttRoom *mqtt_room_;
};

class MqttRoom : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_topic(const std::string &topic) { this->mqtt_topic_ = topic; }
  void add_tracker(MqttRoomTracker *tracker) {
    this->trackers_.push_back(tracker);
    tracker->set_mqtt_room(this);
  }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void send_tracker_update(std::string &id, std::string &name, float distance);

 protected:
  std::string mqtt_topic_;
  std::list<MqttRoomTracker *> trackers_;

  // String helper function
  static std::string format_device_name(const std::string &device_name);
  static std::string format_device_address(const uint8_t *device_address);

  // Device manufacture service UUID
  const esp32_ble_tracker::ESPBTUUID tile_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(0xFEED);

  // Device manufacture service Data UUID
  const esp32_ble_tracker::ESPBTUUID exposure_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(0xFD6F);

  // Device manufacture UUID
  const esp32_ble_tracker::ESPBTUUID apple_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(0x004C);
  const esp32_ble_tracker::ESPBTUUID sonos_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(0x05A7);
  const esp32_ble_tracker::ESPBTUUID samsung_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(0x0075);
  const esp32_ble_tracker::ESPBTUUID google_uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(0x00E0);
};

}  // namespace mqtt_room
}  // namespace esphome

#endif
