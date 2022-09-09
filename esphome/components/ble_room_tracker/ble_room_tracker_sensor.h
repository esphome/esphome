#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/mqtt_room/mqtt_room.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_room_tracker {
class BLERoomTracker : public Component,
                       public esp32_ble_tracker::ESPBTDeviceListener,
                       public mqtt_room::MqttRoomTracker {
 public:
  void set_rssi_sensor(sensor::Sensor *rssi_sensor) { this->rssi_sensor_ = rssi_sensor; }
  void set_signal_power_sensor(sensor::Sensor *signal_power_sensor) {
    this->signal_power_sensor_ = signal_power_sensor;
  }
  void set_distance_sensor(sensor::Sensor *distance_sensor) { this->distance_sensor_ = distance_sensor; }

  void on_scan_end() override {
    if (!this->found_) {
      if (this->rssi_sensor_ != nullptr)
        this->rssi_sensor_->publish_state(NAN);
      if (this->signal_power_sensor_ != nullptr)
        this->signal_power_sensor_->publish_state(NAN);
      if (this->distance_sensor_ != nullptr)
        this->distance_sensor_->publish_state(NAN);
    }

    this->found_ = false;
  }
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device);

  void update_tracker(int rssi, int signal_power = 0);

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  enum MatchType { MATCH_BY_MAC_ADDRESS, MATCH_BY_SERVICE_UUID, MATCH_BY_IBEACON_UUID };
  MatchType match_by_;

  bool found_{false};

  sensor::Sensor *rssi_sensor_;
  sensor::Sensor *signal_power_sensor_;
  sensor::Sensor *distance_sensor_;

  // String helper function
  static std::string format_device_name_(const std::string &device_name);
  static std::string format_device_address_(const uint8_t *device_address);

  // Device manufacture UUID
  const esp32_ble_tracker::ESPBTUUID APPLE_UUID = esp32_ble_tracker::ESPBTUUID::from_uint32(0x004C);
  const esp32_ble_tracker::ESPBTUUID SONOS_UUID = esp32_ble_tracker::ESPBTUUID::from_uint32(0x05A7);
  const esp32_ble_tracker::ESPBTUUID SAMSUNG_UUID = esp32_ble_tracker::ESPBTUUID::from_uint32(0x0075);
  const esp32_ble_tracker::ESPBTUUID GOOGLE_UUID = esp32_ble_tracker::ESPBTUUID::from_uint32(0x00E0);
};
}  // namespace ble_room_tracker
}  // namespace esphome

#endif
