#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <vector>

#ifdef USE_ESP32

namespace esphome {
namespace pvvx_mithermometer {

using PvvxFlags = union {
  uint8_t all;
  struct {
    uint8_t rds_input : 1;
    uint8_t trg_output : 1;
    uint8_t trigger_on : 1;
    uint8_t temp_out_on : 1;
    uint8_t humi_out_on : 1;
  };
};

struct ParseResult {
  optional<float> temperature;
  optional<float> humidity;
  optional<float> battery_level;
  optional<float> battery_voltage;
  optional<PvvxFlags> flags;
  int raw_offset;
};

class PVVXMiThermometer : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }
  void set_battery_voltage(sensor::Sensor *battery_voltage) { battery_voltage_ = battery_voltage; }
  void set_signal_strength(sensor::Sensor *signal_strength) { signal_strength_ = signal_strength; }
  void set_rds_input(binary_sensor::BinarySensor *rds_input) { rds_input_ = rds_input; }
  void set_trg_output(binary_sensor::BinarySensor *trg_output) { trg_output_ = trg_output; }
  void set_trigger_on(binary_sensor::BinarySensor *trigger_on) { trigger_on_ = trigger_on; }
  void set_humi_out_on(binary_sensor::BinarySensor *humi_out_on) { humi_out_on_ = humi_out_on; }
  void set_temp_out_on(binary_sensor::BinarySensor *temp_out_on) { temp_out_on_ = temp_out_on; }

 protected:
  uint64_t address_;
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
  sensor::Sensor *battery_voltage_{nullptr};
  sensor::Sensor *signal_strength_{nullptr};
  binary_sensor::BinarySensor *rds_input_{nullptr};
  binary_sensor::BinarySensor *trg_output_{nullptr};
  binary_sensor::BinarySensor *trigger_on_{nullptr};
  binary_sensor::BinarySensor *humi_out_on_{nullptr};
  binary_sensor::BinarySensor *temp_out_on_{nullptr};

  optional<ParseResult> parse_header_(const esp32_ble_tracker::ServiceData &service_data);
  bool parse_message_(const std::vector<uint8_t> &message, ParseResult &result);
  bool report_results_(const optional<ParseResult> &result, const std::string &address);
};

}  // namespace pvvx_mithermometer
}  // namespace esphome

#endif
