#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

namespace esphome {
namespace ruuvi_ble {

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

optional<RuuviParseResult> parse_ruuvi(const esp32_ble_tracker::ESPBTDevice &device);

class RuuviListener : public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
};

}  // namespace ruuvi_ble
}  // namespace esphome

#endif
