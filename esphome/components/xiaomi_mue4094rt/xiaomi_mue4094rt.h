#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_mue4094rt {

class XiaomiMUE4094RT : public Component,
                        public binary_sensor::BinarySensorInitiallyOff,
                        public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_addr(uint64_t address) { address_ = address; }
  void set_time(uint16_t timeout) { timeout_ = timeout; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    if (device.address_uint64() != this->address_)
      return false;

    auto res = xiaomi_ble::parse_xiaomi(device);
    if (!res.has_value())
      return false;

    if (res->motion.has_value()) {
      this->publish_state(true);
      this->set_timeout("motion_timeout", timeout_, [this]() { this->publish_state(false); });
    }
    return true;
  }

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  uint64_t address_;
  uint16_t timeout_;
};

}  // namespace xiaomi_mue4094rt
}  // namespace esphome

#endif
