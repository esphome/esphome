#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::islandaire {

class IslandaireClimate final : public climate_ir::ClimateIR {
 public:
  // The unit works in whole degrees Fahrenheit (see traits())
  static constexpr uint8_t TEMP_MIN = 61;
  static constexpr uint8_t TEMP_MAX = 88;

  IslandaireClimate()
      : ClimateIR(TEMP_MIN, TEMP_MAX, 1.0f, false, true,
                  {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_HIGH}, {}) {}

 protected:
  void setup() override;
  void dump_config() override;
  climate::ClimateTraits traits() override;
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace esphome::islandaire
