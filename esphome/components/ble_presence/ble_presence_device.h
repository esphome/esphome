#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ble_presence {

class BLEPresenceDevice : public binary_sensor::BinarySensor,
                          public esp32_ble_tracker::ESPBTDeviceListener,
                          public Component {
 public:
  void set_address(uint64_t address) { address_ = address; }
  void on_scan_end() override {
    if (!this->found_)
      this->publish_state(false);
    this->found_ = false;
  }
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    if (device.address_uint64() == this->address_) {
      this->publish_state(true);
      this->found_ = true;
      return true;
    }
    return false;
  }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  bool found_{false};
  uint64_t address_;
};

}  // namespace ble_presence
}  // namespace esphome

#endif
