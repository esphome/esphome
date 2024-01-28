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
  MitsubishiClimate() : climate_ir::ClimateIR(MITSUBISHI_TEMP_MIN, MITSUBISHI_TEMP_MAX) {}

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
};

}  // namespace mitsubishi
}  // namespace esphome
