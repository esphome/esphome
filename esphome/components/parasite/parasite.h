#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

// #ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace parasite {

class Parasite : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };
  void set_bindkey(const std::string &bindkey);

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }

 protected:
  uint64_t address_;
  sensor::Sensor *humidity_{nullptr};
};

}  // namespace parasite
}  // namespace esphome

// #endif
