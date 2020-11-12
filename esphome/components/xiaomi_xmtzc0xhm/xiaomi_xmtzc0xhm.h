#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_xmtzc0xhm {

class XiaomiXMTZC0XHM : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_weight(sensor::Sensor *weight) { weight_ = weight; }
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }
  void set_impedance(sensor::Sensor *impedance) { impedance_ = impedance; }

 protected:
  uint64_t address_;
  sensor::Sensor *weight_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
  sensor::Sensor *impedance_{nullptr};
};

}  // namespace xiaomi_xmtzc0xhm
}  // namespace esphome

#endif
