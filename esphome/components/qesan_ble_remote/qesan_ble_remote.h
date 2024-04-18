#pragma once

#include <vector>

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif  // USE_BINARY_SENSOR

#ifdef USE_ESP32

namespace esphome {
namespace qesan_ble_remote {

#ifdef USE_BINARY_SENSOR
class QesanBinarySensor : public binary_sensor::BinarySensor {
 public:
  void set_button_code(uint8_t button_code) { this->button_code_ = button_code; };
  uint8_t get_button_code() { return this->button_code_; };
  bool on_update_received(uint8_t button_code, bool pressed);

 protected:
  uint8_t button_code_{0};
};
#endif  // USE_BINARY_SENSOR

class QesanListener : public PollingComponent, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void set_mac_address(uint64_t address) {
    mac_address_ = address;
    filter_by_mac_address_ = true;
  };
  void set_remote_address(uint16_t address) {
    remote_address_ = address;
    filter_by_remote_address_ = true;
  };
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(QesanBinarySensor *binary_sensor) { this->binary_sensors_.push_back(binary_sensor); };
#endif  // USE_BINARY_SENSOR
 protected:
  uint8_t last_action_{0xff};
  uint8_t last_xor_mask_{0};
  uint16_t last_remote_address_{0};
  time_t last_message_received_{0};
  uint64_t mac_address_{0};
  uint16_t remote_address_{0};
  bool filter_by_mac_address_{false};
  bool filter_by_remote_address_{false};
#ifdef USE_BINARY_SENSOR
  std::vector<QesanBinarySensor *> binary_sensors_;
#endif  // USE_BINARY_SENSOR
};

}  // namespace qesan_ble_remote
}  // namespace esphome

#endif  // USE_ESP32
