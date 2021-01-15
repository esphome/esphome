#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_miscale {

struct ParseResult {
  optional<float> weight;
};

class XiaomiMiscale : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_weight(sensor::Sensor *weight) { weight_ = weight; }

 protected:
  uint64_t address_;
  sensor::Sensor *weight_{nullptr};

  optional<ParseResult> parse_header(const esp32_ble_tracker::ServiceData &service_data);
  bool parse_message(const std::vector<uint8_t> &message, ParseResult &result);
  bool report_results(const optional<ParseResult> &result, const std::string &address);
};

}  // namespace xiaomi_miscale
}  // namespace esphome

#endif
