#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ble_rssi {

class BLERSSISensor : public sensor::Sensor, public esp32_ble_tracker::ESPBTDeviceListener, public Component {
 public:
  BLERSSISensor(const std::string &name, esp32_ble_tracker::ESP32BLETracker *parent, uint64_t address)
      : sensor::Sensor(name), esp32_ble_tracker::ESPBTDeviceListener(parent), address_(address) {}

  void on_scan_end() override {
    if (!this->found_)
      this->publish_state(NAN);
    this->found_ = false;
  }
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    if (device.address_uint64() == this->address_) {
      this->publish_state(device.get_rssi());
      this->found_ = true;
      return true;
    }
    return false;
  }
  void setup() override { this->setup_ble(); }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  bool found_{false};
  uint64_t address_;
};

}  // namespace ble_rssi
}  // namespace esphome
