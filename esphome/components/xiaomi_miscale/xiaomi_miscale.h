#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <vector>

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_miscale {

struct ParseResult {
  int version;
  optional<float> weight;
  optional<float> impedance;
};

class XiaomiMiscale : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_weight(sensor::Sensor *weight) { weight_ = weight; }
  void set_impedance(sensor::Sensor *impedance) { impedance_ = impedance; }
  void set_clear_impedance(bool clear_impedance) { clear_impedance_ = clear_impedance; }

 protected:
  uint64_t address_;
  sensor::Sensor *weight_{nullptr};
  sensor::Sensor *impedance_{nullptr};
  bool clear_impedance_{false};

  optional<ParseResult> parse_header_(const esp32_ble_tracker::ServiceData &service_data);
  bool parse_message_(const std::vector<uint8_t> &message, ParseResult &result);
  bool parse_message_v1_(const std::vector<uint8_t> &message, ParseResult &result);
  bool parse_message_v2_(const std::vector<uint8_t> &message, ParseResult &result);
  bool report_results_(const optional<ParseResult> &result, const std::string &address);
};

}  // namespace xiaomi_miscale
}  // namespace esphome

#endif
