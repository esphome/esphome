#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::hitachi_168bit {

/// Simple enum to represent models.
enum Model {
  MODEL_HCRA31NEWH = 0,  /// Temperature range is from 16 to 30
};

// Temperature
static constexpr float HITACHI168BIT_HCRA31NEWH_TEMP_MIN = 16.0f;
static constexpr float HITACHI168BIT_HCRA31NEWH_TEMP_MAX = 30.0f;

class Hitachi168bitClimate final : public climate_ir::ClimateIR {
 public:
  Hitachi168bitClimate();

  void setup() override {
    climate_ir::ClimateIR::setup();

    this->powered_on_assumed = this->mode != climate::CLIMATE_MODE_OFF;
  }

  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override {
    this->send_swing_cmd_ = call.get_swing_mode().has_value();
    climate_ir::ClimateIR::control(call);
  }

  void set_model(Model model) {
    this->model_ = model;
    this->minimum_temperature_ = this->temperature_min_();
    this->maximum_temperature_ = this->temperature_max_();
  }

  /// Used to track when to send the power toggle command.
  bool powered_on_assumed{false};

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR buffer.
  bool on_receive(remote_base::RemoteReceiveData data) override;

  /// Time of the last transmission.
  uint32_t last_transmit_time_{};
  /// The remote sends swing as a toggle, only on the call that changed it.
  bool send_swing_cmd_{false};
  Model model_{MODEL_HCRA31NEWH};

  float temperature_min_() const { return HITACHI168BIT_HCRA31NEWH_TEMP_MIN; }
  float temperature_max_() const { return HITACHI168BIT_HCRA31NEWH_TEMP_MAX; }
};

}  // namespace esphome::hitachi_168bit
