#pragma once

#include <iterator>

#include "esphome/components/climate_ir/climate_ir.h"
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#include "panasonic_ac_frame.h"

namespace esphome::panasonic_ir {

static constexpr const char *FAN_MODE_MINIMUM = "Minimum";
static constexpr const char *FAN_MODE_MAXIMUM = "Maximum";
static constexpr const char *const CUSTOM_FAN_MODES[] PROGMEM = {FAN_MODE_MINIMUM, FAN_MODE_MAXIMUM};

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
    this->set_supported_custom_fan_modes_static(CUSTOM_FAN_MODES, std::size(CUSTOM_FAN_MODES));
  }

  void set_model(Model model) { this->model_ = model; }
  void setup() override;
  void dump_config() override;

  /// Set the vertical vane from the select: 0 is auto (swing vertical), 1 (highest) to 5 (lowest) are fixed positions.
  void set_vane_position(size_t index);
#ifdef USE_SELECT
  SUB_SELECT(vane)
#endif

 protected:
  void control(const climate::ClimateCall &call) override;

  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;

  uint8_t fan_speed_() const;
  uint8_t vane_v_() const;
  void sync_vane_select_();
  void apply_received_(const FrameParams &params);

  Model model_{Model::JKE};
  // The remote has no concept of "off" mode, so remember what to send along with the power bit
  Mode last_mode_{Mode::AUTO};
  // CKP models only have a power toggle, so we need to keep track of the state
  bool ckp_is_on_{false};
  // The fixed vane position (1 to 5) used while swing is off. 0 until one is chosen, then heat blows down and the
  // other modes up
  uint8_t vane_position_{0};
};

}  // namespace esphome::panasonic_ir
