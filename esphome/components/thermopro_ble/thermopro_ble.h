#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

namespace esphome::thermopro_ble {

struct ParseResult {
  optional<float> temperature;
  optional<float> external_temperature;
  optional<float> humidity;
  optional<float> battery_level;
};

using DeviceParser = optional<ParseResult> (*)(const uint8_t *data, std::size_t data_size);

class ThermoProBLE : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { this->address_ = address; };

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  void dump_config() override;
  void set_signal_strength(sensor::Sensor *signal_strength) { this->signal_strength_ = signal_strength; }
  void set_temperature(sensor::Sensor *temperature) { this->temperature_ = temperature; }
  void set_external_temperature(sensor::Sensor *external_temperature) {
    this->external_temperature_ = external_temperature;
  }
  void set_humidity(sensor::Sensor *humidity) { this->humidity_ = humidity; }
  void set_battery_level(sensor::Sensor *battery_level) { this->battery_level_ = battery_level; }

 protected:
  uint64_t address_;
  std::string device_name_;
  DeviceParser device_parser_{nullptr};
  sensor::Sensor *signal_strength_{nullptr};
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *external_temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *battery_level_{nullptr};

  void update_device_type_(const std::string &device_name);
};

}  // namespace esphome::thermopro_ble

#endif
