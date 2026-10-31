#include "equitherm.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::equitherm {

ESPHOME_LOG_TAG(TAG, "equitherm");

// Outdoor readings outside this range count as a sensor failure
static constexpr float OUTDOOR_SENSOR_MIN_VALID = -55.0f;
static constexpr float OUTDOOR_SENSOR_MAX_VALID = 50.0f;

void EquithermClimate::setup() {
  this->indoor_sensor_->add_on_state_callback([this](float) { this->compute_and_apply_(); });
  this->outdoor_sensor_->add_on_state_callback([this](float) { this->compute_and_apply_(); });

  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    this->mode = climate::CLIMATE_MODE_HEAT;
    this->compute_and_apply_();
  }
}

void EquithermClimate::control(const climate::ClimateCall &call) {
  if (auto mode = call.get_mode(); mode.has_value())
    this->mode = *mode;
  if (auto target = call.get_target_temperature(); target.has_value()) {
    // The accumulated error belongs to the old target
    if (*target != this->target_temperature) {
      ESP_LOGD(TAG, "Target temperature %.1f°C → %.1f°C, resetting the PID", this->target_temperature, *target);
      this->pid_controller_.reset();
      this->pid_correction_ = 0.0f;
    }
    this->target_temperature = *target;
  }
  if (call.has_custom_preset()) {
    this->set_custom_preset_(call.get_custom_preset());
  } else if (auto preset = call.get_preset(); preset.has_value() && *preset == climate::CLIMATE_PRESET_NONE) {
    this->clear_custom_preset_();
  }
  this->compute_and_apply_();
}

climate::ClimateTraits EquithermClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION);
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT});
  traits.set_visual_min_temperature(15.0f);
  traits.set_visual_max_temperature(25.0f);
  traits.set_visual_temperature_step(0.5f);
#ifdef USE_NUMBER
  if (this->manual_flow_temp_ != nullptr)
    traits.add_supported_preset(climate::CLIMATE_PRESET_NONE);
#endif
  return traits;
}

void EquithermClimate::dump_config() {
  LOG_CLIMATE("", "Equitherm", this);
  ESP_LOGCONFIG(TAG,
                "  Heating curve: hc %.2f, n %.2f, shift %.2f\n"
                "  PID: kp %.3f, ki %.3f, kd %.3f\n"
                "  Flow temperature: %.1f°C to %.1f°C",
                this->hc_, this->n_, this->shift_, this->pid_controller_.kp_, this->pid_controller_.ki_,
                this->pid_controller_.kd_, this->min_flow_temp_, this->max_flow_temp_);
}

float EquithermClimate::curve_flow_temperature_(float t_target, float t_outdoor) const {
  // The standard European heating curve (Buderus/Viessmann style): hc stands for the building's insulation,
  // n for the radiator type (EN 442) and shift is a flat offset
  const float t_flow = t_target + this->shift_ + this->hc_ * powf(t_target - t_outdoor, 1.0f / this->n_);
  return clamp(t_flow, this->min_flow_temp_, this->max_flow_temp_);
}

void EquithermClimate::set_action_(climate::ClimateAction action) {
  if (action == this->action)
    return;
  if (action == climate::CLIMATE_ACTION_HEATING) {
    this->on_heating_start_callback_.call();
  } else {
    if (this->action == climate::CLIMATE_ACTION_HEATING)
      this->on_heating_stop_callback_.call();
    this->write_setpoint_off_();
    this->active_setpoint_ = NAN;
    // The PID is not stepped while heating is off, so its history must not carry over
    this->pid_controller_.reset();
    this->pid_correction_ = 0.0f;
  }
  this->action = action;
}

void EquithermClimate::write_setpoint_(float temp_c) {
#ifdef USE_NUMBER
  if (this->flow_setpoint_output_ != nullptr) {
    this->flow_setpoint_output_->make_call().set_value(temp_c).perform();
    return;
  }
#endif
#ifdef USE_EQUITHERM_HEAT_OUTPUT
  if (this->heat_output_ != nullptr)
    this->heat_output_->set_level((temp_c - this->min_flow_temp_) / (this->max_flow_temp_ - this->min_flow_temp_));
#endif
}

void EquithermClimate::write_setpoint_off_() {
  // A setpoint number has no value that means off (its minimum is still a heat request), so a boiler on a
  // number is turned off from on_heating_stop. A float output is off at 0.
#ifdef USE_EQUITHERM_HEAT_OUTPUT
  if (this->heat_output_ != nullptr)
    this->heat_output_->set_level(0.0f);
#endif
}

void EquithermClimate::compute_and_apply_(bool update_pid) {
  if (std::isnan(this->target_temperature))
    return;

  const bool indoor_valid = this->indoor_sensor_->has_state() && !std::isnan(this->indoor_sensor_->state);
  if (indoor_valid)
    this->current_temperature = this->indoor_sensor_->state;

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->set_action_(climate::CLIMATE_ACTION_OFF);
    this->publish_state();
    return;
  }

  float t_flow;
#ifdef USE_NUMBER
  const bool manual_active = this->has_custom_preset() && this->manual_flow_temp_ != nullptr &&
                             this->manual_flow_temp_->has_state() && !std::isnan(this->manual_flow_temp_->state);
#else
  const bool manual_active = false;
#endif
  if (manual_active) {
#ifdef USE_NUMBER
    t_flow = this->manual_flow_temp_->state;
    this->pid_controller_.reset();
    this->pid_correction_ = 0.0f;
#endif
  } else {
    const float t_outdoor = this->outdoor_sensor_->state;
    // A NAN reading fails both comparisons
    const bool outdoor_valid = this->outdoor_sensor_->has_state() && t_outdoor >= OUTDOOR_SENSOR_MIN_VALID &&
                               t_outdoor <= OUTDOOR_SENSOR_MAX_VALID;
    if (!outdoor_valid) {
      // The curve cannot be computed, so the last setpoint stays; the PID is not stepped meanwhile
      ESP_LOGV(TAG, "Outdoor sensor invalid, holding the last setpoint");
      this->pid_controller_.reset();
      this->pid_correction_ = 0.0f;
      this->publish_state();
      return;
    }
    // Warm weather shutdown: no heating demand once it is as warm outside as the target
    if (this->target_temperature <= t_outdoor) {
      this->set_action_(climate::CLIMATE_ACTION_IDLE);
      this->publish_state();
      return;
    }
    if (!indoor_valid) {
      this->pid_controller_.reset();
      this->pid_correction_ = 0.0f;
    } else if (update_pid) {
      this->pid_correction_ = this->pid_controller_.update(this->target_temperature, this->current_temperature);
    }
    t_flow = this->curve_flow_temperature_(this->target_temperature, t_outdoor) + this->pid_correction_;
  }

  this->set_action_(climate::CLIMATE_ACTION_HEATING);

  // 0.1 °C steps, as a thermostat sends, so noise below that never reaches the output
  t_flow = roundf(clamp(t_flow, this->min_flow_temp_, this->max_flow_temp_) * 10.0f) / 10.0f;
  if (std::isnan(this->active_setpoint_) || fabsf(t_flow - this->active_setpoint_) > this->write_deadband_) {
    ESP_LOGD(TAG, "Flow setpoint %.1f°C", t_flow);
    this->active_setpoint_ = t_flow;
    this->write_setpoint_(t_flow);
  }
  this->publish_state();
}

}  // namespace esphome::equitherm
