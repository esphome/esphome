#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"

#include <vector>

namespace esphome::pvvx_mithermometer {

struct ParseResult {
  optional<float> temperature;
  optional<float> humidity;
  optional<float> battery_level;
  optional<float> battery_voltage;
  int raw_offset;
};

class PVVXMiThermometer final : public Component, public ble_device_base::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }
  void set_battery_voltage(sensor::Sensor *battery_voltage) { battery_voltage_ = battery_voltage; }
  void set_signal_strength(sensor::Sensor *signal_strength) { signal_strength_ = signal_strength; }

 protected:
  uint64_t address_;
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
  sensor::Sensor *battery_voltage_{nullptr};
  sensor::Sensor *signal_strength_{nullptr};

  uint8_t last_frame_count_{0};

  optional<ParseResult> parse_header_(const ble_device_base::ServiceData &service_data);
  bool parse_message_(const std::vector<uint8_t> &message, ParseResult &result);
  bool report_results_(const optional<ParseResult> &result, const char *address);
};

}  // namespace esphome::pvvx_mithermometer
