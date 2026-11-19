#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::whirlpool {

/// Simple enum to represent models.
enum Model {
  MODEL_DG11J1_3A = 0,    /// Temperature range is from 18 to 32
  MODEL_DG11J1_91 = 1,    /// Temperature range is from 16 to 30
  MODEL_HISENSE_ARG = 2,  /// Hisense/BGH (Argentina), 16 to 30
};

// Temperature
const float WHIRLPOOL_DG11J1_3A_TEMP_MAX = 32.0;
const float WHIRLPOOL_DG11J1_3A_TEMP_MIN = 18.0;
const float WHIRLPOOL_DG11J1_91_TEMP_MAX = 30.0;
const float WHIRLPOOL_DG11J1_91_TEMP_MIN = 16.0;

class WhirlpoolClimate final : public climate_ir::ClimateIR {
 public:
  WhirlpoolClimate();

  void setup() override {
    climate_ir::ClimateIR::setup();

    this->powered_on_assumed = this->mode != climate::CLIMATE_MODE_OFF;
  }

  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override {
    send_swing_cmd_ = call.get_swing_mode().has_value();
    climate_ir::ClimateIR::control(call);
  }

  void set_model(Model model) {
    this->model_ = model;
    this->minimum_temperature_ = temperature_min_();
    this->maximum_temperature_ = temperature_max_();
  }

  // used to track when to send the power toggle command
  bool powered_on_assumed;

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  /// False when the model is compiled out, so the compiler drops the Hisense ARG branches.
  bool is_hisense_arg_() const {
#ifdef USE_WHIRLPOOL_HISENSE_ARG
    return this->model_ == MODEL_HISENSE_ARG;
#else
    return false;
#endif
  }
  /// Modes in which the Hisense ARG model sends a fixed temperature.
  bool fixed_temp_mode_() const;
  /// Add the Hisense ARG bytes on top of the common whirlpool frame.
  void encode_hisense_arg_(uint8_t *remote_state, bool power_toggle, uint8_t temp_step) const;
  /// Set the time of the last transmission.
  uint32_t last_transmit_time_{};

  bool send_swing_cmd_{false};  // the DG11J1 remotes send swing as a toggle, only on the call that changed it
  Model model_;

  float temperature_min_() {
    return (model_ == MODEL_DG11J1_3A) ? WHIRLPOOL_DG11J1_3A_TEMP_MIN : WHIRLPOOL_DG11J1_91_TEMP_MIN;
  }
  float temperature_max_() {
    return (model_ == MODEL_DG11J1_3A) ? WHIRLPOOL_DG11J1_3A_TEMP_MAX : WHIRLPOOL_DG11J1_91_TEMP_MAX;
  }
};

}  // namespace esphome::whirlpool
