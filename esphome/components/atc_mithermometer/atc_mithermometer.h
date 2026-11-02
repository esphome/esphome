#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"

#include <vector>

namespace esphome::atc_mithermometer {

struct ParseResult {
  optional<float> temperature;
  optional<float> humidity;
  optional<float> battery_level;
  optional<float> battery_voltage;
  int raw_offset;
};

class ATCMiThermometer final : public Component, public ble_device_base::ESPBTDeviceListener {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(battery_level)
  SUB_SENSOR(battery_voltage)
  SUB_SENSOR(signal_strength)

 public:
  void set_address(uint64_t address) { address_ = address; };

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;

 protected:
  uint64_t address_;

  uint8_t last_frame_count_{0};

  optional<ParseResult> parse_header_(const ble_device_base::ServiceData &service_data);
  bool parse_message_(const std::vector<uint8_t> &message, ParseResult &result);
  bool report_results_(const optional<ParseResult> &result, const char *address);
};

}  // namespace esphome::atc_mithermometer
