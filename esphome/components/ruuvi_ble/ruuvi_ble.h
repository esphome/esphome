#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::ruuvi_ble {

struct RuuviParseResult {
  optional<float> humidity;
  optional<float> temperature;
  optional<float> pressure;
  optional<float> acceleration;
  optional<float> acceleration_x;
  optional<float> acceleration_y;
  optional<float> acceleration_z;
  optional<float> battery_voltage;
  optional<float> tx_power;
  optional<float> movement_counter;
  optional<float> measurement_sequence_number;
};

bool parse_ruuvi_data_byte(uint8_t data_type, const uint8_t *data, uint8_t data_length, RuuviParseResult &result);

optional<RuuviParseResult> parse_ruuvi(const ble_device_base::ESPBTDevice &device);

class RuuviListener final : public ble_device_base::ESPBTDeviceListener {
 public:
  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
};

}  // namespace esphome::ruuvi_ble
