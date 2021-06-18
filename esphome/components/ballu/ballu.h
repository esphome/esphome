#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace ballu {

// Support for Ballu air conditioners with YKR-K/002E remote

// Temperature
const float YKR_K_002E_TEMP_MIN = 16.0;
const float YKR_K_002E_TEMP_MAX = 32.0;

class BalluClimate : public climate_ir::ClimateIR {
 public:
  BalluClimate()
      : climate_ir::ClimateIR(YKR_K_002E_TEMP_MIN, YKR_K_002E_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace ballu
}  // namespace esphome
