#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::thermopro_ble {

struct ParseResult {
  optional<float> temperature;
  optional<float> external_temperature;
  optional<float> humidity;
  optional<float> battery_level;
};

using DeviceParser = optional<ParseResult> (*)(const uint8_t *data, std::size_t data_size);

class ThermoProBLE final : public Component, public ble_device_base::ESPBTDeviceListener {
  SUB_SENSOR(signal_strength)
  SUB_SENSOR(temperature)
  SUB_SENSOR(external_temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(battery_level)

 public:
  void set_address(uint64_t address) { this->address_ = address; };

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;

 protected:
  uint64_t address_;
};

}  // namespace esphome::thermopro_ble
