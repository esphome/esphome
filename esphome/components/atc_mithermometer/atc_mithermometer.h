#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace atc_mithermometer {

struct ParseResult {
  optional<float> temperature;
  optional<float> humidity;
  optional<float> battery_level;
  bool is_duplicate;
  int raw_offset;
};

class ATCMiThermometer : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; };

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }

  optional<ParseResult> parse_header(const esp32_ble_tracker::ServiceData &service_data);
  bool parse_message(const std::vector<uint8_t> &message, ParseResult &result);
  bool report_results(const optional<ParseResult> &result, const std::string &address);

 protected:
  uint64_t address_;
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
};

}  // namespace atc_mithermometer
}  // namespace esphome

#endif
