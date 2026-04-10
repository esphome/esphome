#pragma once

#include <array>

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace whirlpool {

/**
 * @brief Simple enum to represent models.
 */
enum Model {
  MODEL_DG11J1_3A = 0, /* Temperature range is from 18 to 32 (default) */
  MODEL_DG11J1_91,     /* Temperature range is from 16 to 30 */
  MODEL_DG11J1_39,     /* Temperature range is from 18 to 32 */
  MODEL_COUNT
};

class WhirlpoolClimate : public climate_ir::ClimateIR {
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
    auto &model_settings = MODEL_SETTINGS_ARR[model];
    set_minimum_temperature(model_settings.minimum_temperature);
    set_maximum_temperature(model_settings.maximum_temperature);
    temperature_correction_ = model_settings.temperature_correction;
  }

  // used to track when to send the power toggle command
  bool powered_on_assumed;

 protected:
  struct ModelSettings {
    float minimum_temperature;
    float maximum_temperature;
    float temperature_correction;
  };

  static constexpr std::array<ModelSettings, Model::MODEL_COUNT> MODEL_SETTINGS_ARR = {
      ModelSettings(18.f, 32.f, 0.f), /* MODEL_DG11J1_3A */
      ModelSettings(16.f, 30.f, 0.f), /* MODEL_DG11J1_91 */
      ModelSettings(18.f, 32.f, 2.f)  /* MODEL_DG11J1_39 */
  };

  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  /// Set the time of the last transmission.
  uint32_t last_transmit_time_{};

  float temperature_correction_{0.f};

  bool send_swing_cmd_{false};
};

}  // namespace whirlpool
}  // namespace esphome
