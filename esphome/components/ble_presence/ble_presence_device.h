#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_presence {

class BLEPresenceDevice : public binary_sensor::BinarySensorInitiallyOff,
                          public esp32_ble_tracker::ESPBTDeviceListener,
                          public Component {
 public:
  void set_address(uint64_t address) {
    this->match_by_ = MATCH_BY_MAC_ADDRESS;
    this->address_ = address;
  }
  void set_irk(uint8_t *irk) {
    this->match_by_ = MATCH_BY_IRK;
    this->irk_ = irk;
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
  void set_minimum_rssi(int rssi) {
    this->check_minimum_rssi_ = true;
    this->minimum_rssi_ = rssi;
  }
  void set_timeout(uint32_t timeout) { this->timeout_ = timeout; }
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    if (this->check_minimum_rssi_ && this->minimum_rssi_ > device.get_rssi()) {
      return false;
    }
    switch (this->match_by_) {
      case MATCH_BY_MAC_ADDRESS:
        if (device.address_uint64() == this->address_) {
          this->set_found_(true);
          return true;
        }
        break;
      case MATCH_BY_IRK:
        if (device.resolve_irk(this->irk_)) {
          this->set_found_(true);
          return true;
        }
        break;
      case MATCH_BY_SERVICE_UUID:
        for (auto uuid : device.get_service_uuids()) {
          if (this->uuid_ == uuid) {
            this->set_found_(true);
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

        this->set_found_(true);
        return true;
    }
    return false;
  }

  void loop() override {
    if (this->found_ && this->last_seen_ + this->timeout_ < millis())
      this->set_found_(false);
  }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void set_found_(bool state) {
    this->found_ = state;
    if (state)
      this->last_seen_ = millis();
    this->publish_state(state);
  }
  enum MatchType { MATCH_BY_MAC_ADDRESS, MATCH_BY_IRK, MATCH_BY_SERVICE_UUID, MATCH_BY_IBEACON_UUID };
  MatchType match_by_;

  uint64_t address_;
  uint8_t *irk_;

  esp32_ble_tracker::ESPBTUUID uuid_;

  esp32_ble_tracker::ESPBTUUID ibeacon_uuid_;
  uint16_t ibeacon_major_{0};
  uint16_t ibeacon_minor_{0};

  int minimum_rssi_{0};

  bool check_ibeacon_major_{false};
  bool check_ibeacon_minor_{false};
  bool check_minimum_rssi_{false};

  bool found_{false};
  uint32_t last_seen_{};
  uint32_t timeout_{};
};

}  // namespace ble_presence
}  // namespace esphome

#endif
