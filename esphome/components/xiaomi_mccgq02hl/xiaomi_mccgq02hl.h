#pragma once

#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"

#include <vector>

namespace esphome::xiaomi_mccgq02hl {

// Xiaomi Mijia Door/Window Sensor 2 (MCCGQ02HL), MiBeacon product id 0x098b.
//
// Self-contained on purpose: xiaomi_ble's parse_xiaomi_header() does not know
// this product id and parse_xiaomi_value() does not know the door object, so
// this class does its own header and object parsing and decryption.
class XiaomiMCCGQ02HL final : public Component,
                              public binary_sensor::BinarySensorInitiallyOff,
                              public ble_device_base::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { this->address_ = address; }
  void set_bindkey(const char *bindkey);
  void set_light(binary_sensor::BinarySensor *light) { this->light_ = light; }
  void set_battery_level(sensor::Sensor *battery_level) { this->battery_level_ = battery_level; }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;

 protected:
  struct Reading {
    optional<bool> open;
    optional<bool> light;
    optional<float> battery_level;
  };

  bool decrypt_(const uint8_t *frame, size_t size, size_t offset, uint8_t *plaintext) const;
  bool parse_service_data_(const std::vector<uint8_t> &data, Reading &reading);
  bool parse_objects_(const uint8_t *payload, size_t length, Reading &reading);

  uint64_t address_{0};
  uint8_t bindkey_[16]{};
  // Per instance, unlike xiaomi_ble's function-static counter which is shared
  // by every Xiaomi device on the node.
  optional<uint8_t> last_frame_count_;
  binary_sensor::BinarySensor *light_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
};

}  // namespace esphome::xiaomi_mccgq02hl
