#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"

#ifdef ARDUINO_ARCH_ESP32

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
      if (this->uuid_.get_uuid().len == ESP_UUID_LEN_128) {
        auto ibeacon = device.get_ibeacon();
        if (ibeacon.has_value()) {
          auto uuid = ibeacon.value().get_uuid();
          if (uuid.get_uuid().len == ESP_UUID_LEN_128) {
            bool found = true;
            for (int i = 0; i < ESP_UUID_LEN_128; i++) {
              if (this->uuid_.get_uuid().uuid.uuid128[i] != uuid.get_uuid().uuid.uuid128[i]) {
                found = false;
                break;
              }
            }
            if (found) {
              this->publish_state(device.get_rssi());
              this->found_ = true;
              return true;
            }
          }
        }
      }

      for (auto uuid : device.get_service_uuids()) {
        switch (this->uuid_.get_uuid().len) {
          case ESP_UUID_LEN_16:
            if (uuid.get_uuid().len == ESP_UUID_LEN_16 &&
                uuid.get_uuid().uuid.uuid16 == this->uuid_.get_uuid().uuid.uuid16) {
              this->publish_state(device.get_rssi());
              this->found_ = true;
              return true;
            }
            break;
          case ESP_UUID_LEN_32:
            if (uuid.get_uuid().len == ESP_UUID_LEN_32 &&
                uuid.get_uuid().uuid.uuid32 == this->uuid_.get_uuid().uuid.uuid32) {
              this->publish_state(device.get_rssi());
              this->found_ = true;
              return true;
            }
            break;
          case ESP_UUID_LEN_128:
            if (uuid.get_uuid().len == ESP_UUID_LEN_128) {
              for (int i = 0; i < ESP_UUID_LEN_128; i++) {
                if (uuid.get_uuid().uuid.uuid128[i] != this->uuid_.get_uuid().uuid.uuid128[i]) {
                  return false;
                }
              }
              this->publish_state(device.get_rssi());
              this->found_ = true;
              return true;
            }
            break;
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
