#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace electra_rc3_ir {

// Temperature
const uint8_t ELECTRA_RC3_TEMP_MIN = 16;
const uint8_t ELECTRA_RC3_TEMP_MAX = 30;

class ElectraRC3IR : public climate_ir::ClimateIR {
 public:
  ElectraRC3IR()
      : climate_ir::ClimateIR(ELECTRA_RC3_TEMP_MIN, ELECTRA_RC3_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
                              {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_SLEEP}) {}

  // void control(const climate::ClimateCall &call) override;

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;

 private:
  climate::ClimateMode current_mode_{climate::ClimateMode::CLIMATE_MODE_OFF};
};

}  // namespace electra_rc3_ir
}  // namespace esphome
