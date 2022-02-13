#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_rssi {

class BLERSSISensor : public sensor::Sensor, public esp32_ble_tracker::ESPBTDeviceListener, public Component {
 public:
  void set_address(uint64_t address) {
    this->by_address_ = true;
    this->address_ = address;
  }
  void set_service_uuid16(uint16_t uuid) {
    this->by_address_ = false;
    this->uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint16(uuid);
  }
  void set_service_uuid32(uint32_t uuid) {
    this->by_address_ = false;
    this->uuid_ = esp32_ble_tracker::ESPBTUUID::from_uint32(uuid);
  }
  void set_service_uuid128(uint8_t *uuid) {
    this->by_address_ = false;
    this->uuid_ = esp32_ble_tracker::ESPBTUUID::from_raw(uuid);
  }
  void on_scan_end() override {
    if (!this->found_)
      this->publish_state(NAN);
    this->found_ = false;
  }
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    if (this->by_address_) {
      if (device.address_uint64() == this->address_) {
        this->publish_state(device.get_rssi());
        this->found_ = true;
        return true;
      }
    } else {
      for (auto uuid : device.get_service_uuids()) {
        if (this->uuid_ == uuid) {
          this->publish_state(device.get_rssi());
          this->found_ = true;
          return true;
        }
      }
    }
    return false;
  }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  bool found_{false};
  bool by_address_{false};
  uint64_t address_;
  esp32_ble_tracker::ESPBTUUID uuid_;
};

}  // namespace ble_rssi
}  // namespace esphome

#endif
