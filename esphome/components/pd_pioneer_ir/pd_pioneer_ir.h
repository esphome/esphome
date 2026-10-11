#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "pd_pioneer_data.h"

namespace esphome::pd_pioneer_ir {

class PDPioneerIR : public climate_ir::ClimateIR {
 public:
  PDPioneerIR()
      : climate_ir::ClimateIR(
            PDPIONEER_TEMPC_MIN, PDPIONEER_TEMPC_MAX, 1.0f, true, true,
            {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
             climate::CLIMATE_FAN_HIGH},
            {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL},
            {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_ECO}) {}

  void control(const climate::ClimateCall &call) override;
  void setup() override;
  climate::ClimateTraits traits() override;

  void set_fahrenheit(bool value) { this->temperature_step_ = value ? 0.5f : 1.0f; }

 protected:
  void transmit_state() override;
  void transmit_frame_(const remote_base::PDPioneerData &frame);
  void transmit_pair_(const ControlData &data);
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool apply_frame_(const remote_base::PDPioneerData &frame);

  bool eco_{false};
  ControlData rx_state_;
  bool rx_odd_pending_{false};
};

}  // namespace esphome::pd_pioneer_ir
