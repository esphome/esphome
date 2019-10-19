#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace coolix {

// Temperature
const uint8_t COOLIX_TEMP_MIN = 17;  // Celsius
const uint8_t COOLIX_TEMP_MAX = 30;  // Celsius

class CoolixClimate : public climate::ClimateIR {
 public:
  CoolixClimate() : climate::ClimateIR(COOLIX_TEMP_MIN, COOLIX_TEMP_MAX) {}

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace coolix
}  // namespace esphome
