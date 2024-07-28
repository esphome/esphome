#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace hitachi_168bit {

/// Simple enum to represent models.
enum Model {
  MODEL_DG11J1_91 = 1,  /// Temperature range is from 16 to 30
};

// Temperature
const float hitachi_168bit_DG11J1_91_TEMP_MAX = 30.0;
const float hitachi_168bit_DG11J1_91_TEMP_MIN = 16.0;

class hitachi_168bitClimate : public climate_ir::ClimateIR {
 public:
  hitachi_168bitClimate()
      : climate_ir::ClimateIR(temperature_min_(), temperature_max_(), 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  void setup() override {
    climate_ir::ClimateIR::setup();

    this->powered_on_assumed = this->mode != climate::CLIMATE_MODE_OFF;
  }

  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override {
    send_swing_cmd_ = call.get_swing_mode().has_value();
    climate_ir::ClimateIR::control(call);
  }

  void set_model(Model model) { this->model_ = model; }

  // used to track when to send the power toggle command
  bool powered_on_assumed;

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  /// Set the time of the last transmission.
  int32_t last_transmit_time_{};

  bool send_swing_cmd_{false};
  Model model_;

  float temperature_min_() {
    return (model_ == MODEL_DG11J1_3A) ? hitachi_168bit_DG11J1_3A_TEMP_MIN : hitachi_168bit_DG11J1_91_TEMP_MIN;
  }
  float temperature_max_() {
    return (model_ == MODEL_DG11J1_3A) ? hitachi_168bit_DG11J1_3A_TEMP_MAX : hitachi_168bit_DG11J1_91_TEMP_MAX;
  }
};

}  // namespace hitachi_168bit
}  // namespace esphome
