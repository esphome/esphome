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
  void set_address(uint64_t address) {
    this->match_by_ = MATCH_BY_MAC_ADDRESS;
    this->address_ = address;
  }
  void set_service_uuid16(uint16_t uuid) {
    this->match_by_ = MATCH_BY_SERVICE_UUID;
    this->uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint16(uuid);
  }
  void set_service_uuid32(uint32_t uuid) {
    this->match_by_ = MATCH_BY_SERVICE_UUID;
    this->uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(uuid);
  }
  void set_service_uuid128(uint8_t *uuid) {
    this->match_by_ = MATCH_BY_SERVICE_UUID;
    this->uuid_ = esp32_ble_tracker::ESPBTUUID::from_raw(uuid);
  }
  void set_ibeacon_uuid(uint8_t *uuid) {
    this->match_by_ = MATCH_BY_IBEACON_UUID;
    this->ibeacon_uuid_ = esp32_ble_tracker::ESPBTUUID::from_raw(uuid);
  }
  void set_ibeacon_major(uint16_t major) {
    this->check_ibeacon_major_ = true;
    this->ibeacon_major_ = major;
  }
  void set_ibeacon_minor(uint16_t minor) {
    this->check_ibeacon_minor_ = true;
    this->ibeacon_minor_ = minor;
  }

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
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    // TODO: Rewrite to support more devices
    switch (this->match_by_) {
      case MATCH_BY_MAC_ADDRESS:
        if (device.address_uint64() == this->address_) {
          this->update_tracker(device.get_rssi());
          this->found_ = true;
          return true;
        }
        break;
      case MATCH_BY_SERVICE_UUID:
        for (auto uuid : device.get_service_uuids()) {
          if (this->uuid_ == uuid) {
            this->update_tracker(device.get_rssi());
            this->found_ = true;
            return true;
          }
        }
        break;
      case MATCH_BY_IBEACON_UUID:
        if (!device.get_ibeacon().has_value()) {
          return false;
        }

        auto ibeacon = device.get_ibeacon().value();

        if (this->ibeacon_uuid_ != ibeacon.get_uuid()) {
          return false;
        }

        if (this->check_ibeacon_major_ && this->ibeacon_major_ != ibeacon.get_major()) {
          return false;
        }

        if (this->check_ibeacon_minor_ && this->ibeacon_minor_ != ibeacon.get_minor()) {
          return false;
        }

        this->update_tracker(device.get_rssi(), ibeacon.get_signal_power());
        this->found_ = true;
        return true;
    }
    return false;
  }

  void update_tracker(int rssi, int signal_power = 0);

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  enum MatchType { MATCH_BY_MAC_ADDRESS, MATCH_BY_SERVICE_UUID, MATCH_BY_IBEACON_UUID };
  MatchType match_by_;

  bool found_{false};

  uint64_t address_;

  esp32_ble_tracker::ESPBTUUID uuid_;

  esp32_ble_tracker::ESPBTUUID ibeacon_uuid_;
  uint16_t ibeacon_major_;
  bool check_ibeacon_major_;
  uint16_t ibeacon_minor_;
  bool check_ibeacon_minor_;

  sensor::Sensor *rssi_sensor_;
  sensor::Sensor *signal_power_sensor_;
  sensor::Sensor *distance_sensor_;
};
}  // namespace ble_room_tracker
}  // namespace esphome

#endif
