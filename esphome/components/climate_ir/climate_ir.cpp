#include "climate_ir.h"
#include "esphome/core/log.h"

namespace esphome {
namespace climate_ir {

static const char *TAG = "climate_ir";

climate::ClimateTraits ClimateIR::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(this->sensor_ != nullptr);
  traits.set_supports_heat_cool_mode(true);
  traits.set_supports_cool_mode(this->supports_cool_);
  traits.set_supports_heat_mode(this->supports_heat_);
  traits.set_supports_dry_mode(this->supports_dry_);
  traits.set_supports_fan_only_mode(this->supports_fan_only_);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_away(false);
  traits.set_visual_min_temperature(this->minimum_temperature_);
  traits.set_visual_max_temperature(this->maximum_temperature_);
  traits.set_visual_temperature_step(this->temperature_step_);
  for (auto fan_mode : this->fan_modes_) {
    switch (fan_mode) {
      case climate::CLIMATE_FAN_AUTO:
        traits.set_supports_fan_mode_auto(true);
        break;
      case climate::CLIMATE_FAN_DIFFUSE:
        traits.set_supports_fan_mode_diffuse(true);
        break;
      case climate::CLIMATE_FAN_FOCUS:
        traits.set_supports_fan_mode_focus(true);
        break;
      case climate::CLIMATE_FAN_HIGH:
        traits.set_supports_fan_mode_high(true);
        break;
      case climate::CLIMATE_FAN_LOW:
        traits.set_supports_fan_mode_low(true);
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        traits.set_supports_fan_mode_medium(true);
        break;
      case climate::CLIMATE_FAN_MIDDLE:
        traits.set_supports_fan_mode_middle(true);
        break;
      case climate::CLIMATE_FAN_OFF:
        traits.set_supports_fan_mode_off(true);
        break;
      case climate::CLIMATE_FAN_ON:
        traits.set_supports_fan_mode_on(true);
        break;
    }
  }
  for (auto swing_mode : this->swing_modes_) {
    switch (swing_mode) {
      case climate::CLIMATE_SWING_OFF:
        traits.set_supports_swing_mode_off(true);
        break;
      case climate::CLIMATE_SWING_BOTH:
        traits.set_supports_swing_mode_both(true);
        break;
      case climate::CLIMATE_SWING_VERTICAL:
        traits.set_supports_swing_mode_vertical(true);
        break;
      case climate::CLIMATE_SWING_HORIZONTAL:
        traits.set_supports_swing_mode_horizontal(true);
        break;
    }
  }
  return traits;
}

void ClimateIR::setup() {
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      // current temperature changed, publish state
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  } else
    this->current_temperature = NAN;
  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->apply(this);
  } else {
    // restore from defaults
    this->mode = climate::CLIMATE_MODE_OFF;
    // initialize target temperature to some value so that it's not NAN
    this->target_temperature =
        roundf(clamp(this->current_temperature, this->minimum_temperature_, this->maximum_temperature_));
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }
  // Never send nan to HA
  if (isnan(this->target_temperature))
    this->target_temperature = 24;
}

void ClimateIR::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();
  if (call.get_fan_mode().has_value())
    this->fan_mode = *call.get_fan_mode();
  if (call.get_swing_mode().has_value())
    this->swing_mode = *call.get_swing_mode();
  this->transmit_state();
  this->publish_state();
}
void ClimateIR::dump_config() {
  LOG_CLIMATE("", "IR Climate", this);
  ESP_LOGCONFIG(TAG, "  Min. Temperature: %.1f°C", this->minimum_temperature_);
  ESP_LOGCONFIG(TAG, "  Max. Temperature: %.1f°C", this->maximum_temperature_);
  ESP_LOGCONFIG(TAG, "  Supports HEAT: %s", YESNO(this->supports_heat_));
  ESP_LOGCONFIG(TAG, "  Supports COOL: %s", YESNO(this->supports_cool_));
}

}  // namespace climate_ir
}  // namespace esphome
