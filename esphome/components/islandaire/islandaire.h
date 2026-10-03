#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::islandaire {

// Temperature limits in Celsius corresponding to 61°F - 88°F
const float ISLANDAIRE_TEMP_MIN = 16.11f;  // 61°F
const float ISLANDAIRE_TEMP_MAX = 31.11f;  // 88°F

class IslandaireClimate final : public climate_ir::ClimateIR {
 public:
  IslandaireClimate()
      : climate_ir::ClimateIR(ISLANDAIRE_TEMP_MIN, ISLANDAIRE_TEMP_MAX, 0.5f, false, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_HIGH}, {}) {}

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace esphome::islandaire
