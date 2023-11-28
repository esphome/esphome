#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

#include <cinttypes>

namespace esphome {
namespace mitsubishi {

// Temperature
const uint8_t MITSUBISHI_TEMP_MIN = 16;  // Celsius
const uint8_t MITSUBISHI_TEMP_MAX = 31;  // Celsius

class MitsubishiClimate : public climate_ir::ClimateIR {
 public:
  MitsubishiClimate()
      : climate_ir::ClimateIR(
            MITSUBISHI_TEMP_MIN, MITSUBISHI_TEMP_MAX, 1.0f, true, true,
            {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MIDDLE,
             climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET},
            {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL,
             climate::CLIMATE_SWING_BOTH},
            {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_ECO, climate::CLIMATE_PRESET_BOOST}) {}

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
};

}  // namespace mitsubishi
}  // namespace esphome
