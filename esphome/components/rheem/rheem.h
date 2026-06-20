#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::rheem {

// Temperature
const float RHEEM_TEMP_MAX = 31.0;
const float RHEEM_TEMP_MIN = 16.0;

class RheemClimate : public climate_ir::ClimateIR {
 public:
  RheemClimate()
      : climate_ir::ClimateIR(RHEEM_TEMP_MIN, RHEEM_TEMP_MAX, .5f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {
    this->mute = true;
  }

  bool mute{true};  // sets mute to true, its used to lower the fan speed below min

  // Exposed function for YAML switch component to manipulate states
  void set_mute_state(bool state) {
    this->mute = state;
    this->transmit_state();
  }

  // climate::ClimateTraits traits() override;
 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace esphome::rheem
