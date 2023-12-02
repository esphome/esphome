#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_mue4094rt {

class XiaomiMUE4094RT : public Component,
                        public binary_sensor::BinarySensorInitiallyOff,
                        public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_time(uint16_t timeout) { timeout_ = timeout; }

 protected:
  uint64_t address_;
  uint16_t timeout_;
};

}  // namespace xiaomi_mue4094rt
}  // namespace esphome

#endif
