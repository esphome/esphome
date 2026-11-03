#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "pd_pioneer_data.h"

namespace esphome::pd_pioneer_ir {

class PDPioneerIR : public climate_ir::ClimateIR {
 public:
  PDPioneerIR()
      : climate_ir::ClimateIR(PDPIONEER_TEMPC_MIN, PDPIONEER_TEMPC_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH},
                              {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_ECO}) {}

  void setup() override;

  void set_fahrenheit(bool value) { this->temperature_step_ = value ? 0.5f : 1.0f; }

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  // Horizontal swing from the odd burst, applied to the even burst that follows it
  bool rx_horizontal_swing_{false};
};

}  // namespace esphome::pd_pioneer_ir
