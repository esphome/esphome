#include "pid_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pid {

static const char *TAG = "pid.climate";

void PIDClimate::setup() {
  this->controller_.sample_time = this->update_interval_ / 1000.0;
  this->sensor_->add_on_state_callback([this](float state) {
    this->current_temperature = state;
    // current temperature changed, publish state
    // but do not re-compute values - PID controller is re-evaluated at constant interval
    this->publish_state();
  });
  this->current_temperature = this->sensor_->state;
  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    // restore from defaults, change_away handles those for us
    this->mode = climate::CLIMATE_MODE_AUTO;
    this->target_temperature = this->default_target_temperature_;
  }
}
void PIDClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->mode = *call.get_mode();
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  // If switching to non-auto mode, set output immediately
  if (this->mode != climate::CLIMATE_MODE_AUTO)
    this->handle_non_auto_mode_();

  this->publish_state();
}
climate::ClimateTraits PIDClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_supports_auto_mode(true);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_cool_mode(this->cool_output_ != nullptr);
  traits.set_supports_heat_mode(this->heat_output_ != nullptr);
  traits.set_supports_action(true);
  return traits;
}
void PIDClimate::dump_config() { LOG_CLIMATE("", "PID Climate", this); }
void PIDClimate::write_output_(float value) {
  this->output_value_ = value;

  // value < 0 means cool, > 0 means heat
  if (this->cool_output_ != nullptr) {
    float cool_value = clamp(-value, 0.0, 1.0f);
    this->cool_output_->set_level(cool_value);
  }
  if (this->heat_output_ != nullptr) {
    float heat_value = clamp(value, 0.0, 1.0f);
    this->heat_output_->set_level(heat_value);
  }

  // Update action variable for user feedback what's happening
  climate::ClimateAction new_action;
  if (value < 0.0 && this->cool_output_ != nullptr)
    new_action = climate::CLIMATE_ACTION_COOLING;
  else if (value > 0.0 && this->heat_output_ != nullptr)
    new_action = climate::CLIMATE_ACTION_HEATING;
  else if (this->mode == climate::CLIMATE_MODE_OFF)
    new_action = climate::CLIMATE_ACTION_OFF;
  else
    new_action = climate::CLIMATE_ACTION_IDLE;

  if (new_action != this->action) {
    this->action = new_action;
    this->publish_state();
  }
  this->pid_computed_callback_.call();
}
void PIDClimate::handle_non_auto_mode_() {
  // in non-auto mode, switch directly to appropriate action
  //  - HEAT mode / COOL mode -> Output at ±100%
  //  - OFF mode -> Output at 0%
  if (this->mode == climate::CLIMATE_MODE_HEAT) {
    this->write_output_(1.0);
  } else if (this->mode == climate::CLIMATE_MODE_COOL) {
    this->write_output_(-1.0);
  } else if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->write_output_(0.0);
  } else {
    assert(false);
  }
}
void PIDClimate::update() {
  double value;
  if (isnan(this->current_temperature) || isnan(this->target_temperature)) {
    // if any control parameters are nan, turn off all outputs
    value = 0.0;
  } else {
    // Update PID controller irrespective of current mode, to not mess up D/I terms
    // In non-auto mode, we just discard the output value
    value = this->controller_.update(this->target_temperature, this->current_temperature);
  }

  if (this->mode != climate::CLIMATE_MODE_AUTO) {
    this->handle_non_auto_mode_();
    return;
  }

  this->write_output_(value);
}

}  // namespace pid
}  // namespace esphome
