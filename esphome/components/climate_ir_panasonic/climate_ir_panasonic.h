#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "panasonic_ac_frame.h"

namespace esphome::climate_ir_panasonic {

static constexpr const char *FAN_MODE_MINIMUM = "Minimum";
static constexpr const char *FAN_MODE_MAXIMUM = "Maximum";

class PanasonicIrClimate final : public climate_ir::ClimateIR {
 public:
  PanasonicIrClimate()
      : climate_ir::ClimateIR(TEMP_MIN, TEMP_MAX,
                              1.0f,  // Temp step
                              true,  // Supports dry
                              true,  // Supports fan only
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
                              {climate::CLIMATE_PRESET_AWAY}) {
    this->set_supported_custom_fan_modes({FAN_MODE_MINIMUM, FAN_MODE_MAXIMUM});
  }

  void set_model(Model model) { this->model_ = model; }
  void dump_config() override;

 protected:
  void control(const climate::ClimateCall &call) override;

  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;

  uint8_t fan_speed_() const;
  void send_section_(const uint8_t *bytes, size_t len, remote_base::RemoteTransmitData *data, bool gap);
  void apply_received_(const FrameParams &params);

  Model model_{Model::JKE};
  // The remote has no concept of "off" mode, so remember what to send along with the power bit
  Mode last_mode_{Mode::AUTO};
  // CKP models only have a power toggle, so we need to keep track of the state
  bool ckp_is_on_{false};
};

}  // namespace esphome::climate_ir_panasonic
