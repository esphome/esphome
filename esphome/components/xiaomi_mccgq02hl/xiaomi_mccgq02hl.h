#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_mccgq02hl {

class XiaomiMCCGQ02HL : public Component,
                        public binary_sensor::BinarySensorInitiallyOff,
                        public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }
  void set_bindkey(const std::string &bindkey);

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_light(binary_sensor::BinarySensor *light) { is_light_ = light; }
  void set_lock(binary_sensor::BinarySensor *lock) { is_lock_ = lock; }

 protected:
  uint64_t address_;
  uint8_t bindkey_[16];
  binary_sensor::BinarySensor *is_light_{nullptr};
  binary_sensor::BinarySensor *is_lock_{nullptr};
};

}  // namespace xiaomi_mccgq02hl
}  // namespace esphome

#endif
