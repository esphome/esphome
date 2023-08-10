#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace mitsubishi {

// Temperature
const uint8_t MITSUBISHI_TEMP_MIN = 16;  // Celsius
const uint8_t MITSUBISHI_TEMP_MAX = 31;  // Celsius

const std::set<climate::ClimateFanMode> SUPPORTED_FANS = {
    climate::CLIMATE_FAN_LOW,   climate::CLIMATE_FAN_MIDDLE, climate::CLIMATE_FAN_HIGH,
    climate::CLIMATE_FAN_FOCUS, climate::CLIMATE_FAN_QUIET,  climate::CLIMATE_FAN_AUTO,
};

const std::set<climate::ClimateSwingMode> SUPPORTED_SWINGS = {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH,
                                                              climate::CLIMATE_SWING_VERTICAL,
                                                              climate::CLIMATE_SWING_HORIZONTAL};

class MitsubishiClimate : public climate_ir::ClimateIR {
 public:
  MitsubishiClimate()
      : climate_ir::ClimateIR(MITSUBISHI_TEMP_MIN, MITSUBISHI_TEMP_MAX, 1.0f, false, false, SUPPORTED_FANS,
                              SUPPORTED_SWINGS) {}

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
};

}  // namespace mitsubishi
}  // namespace esphome
