#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace ballu_old {

// Support for old Ballu air conditioners
// Tested: BSC-09H

// Temperature
const float YKR_K_002E_TEMP_MIN = 16.0;
const float YKR_K_002E_TEMP_MAX = 31.0;

class BalluOldClimate : public climate_ir::ClimateIR {
 public:
  BalluOldClimate()
      : climate_ir::ClimateIR(YKR_K_002E_TEMP_MIN, YKR_K_002E_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
};

}  // namespace ballu_old
}  // namespace esphome
