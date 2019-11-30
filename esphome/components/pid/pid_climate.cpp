#include "pid_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pid {

static const char *TAG = "pid.climate";

void PidClimate::setup() {
  this->sensor_->add_on_state_callback([this](float state) {
    this->current_temperature = state;
    // current temperature changed, publish state
    this->publish_state();
  });
  
  this->current_temperature = this->sensor_->state;
  // restore set points
  auto restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    this->mode = climate::CLIMATE_MODE_OFF;
  }
}

void PidClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->switch_to_mode_(*call.get_mode());
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  this->publish_state();
}

void PidClimate::update() {
  if (this->mode != climate::CLIMATE_MODE_AUTO 
      || isnan(this->current_temperature) 
      || isnan(this->target_temperature) 
      || isnan(this->previous_temperature_)) {
    this->previous_temperature_ = this->current_temperature;
    return;
  }
  
  double prev_error = this->target_temperature - this->previous_temperature_;
  double error = this->target_temperature - this->current_temperature;
  double d_err = error - prev_error;

  if (error < this->tuning_params_.i_enable) {
    // Clamp integrator to prevent integral windup
    this->i_err_ = min(this->i_err_ + error, static_cast<double>(this->tuning_params_.i_max));
  } else {
    // Error is too large, disable integral accumulation
    this->i_err_ = 0;
  }

  double proportional = (this->tuning_params_.kp * error);
  double integral = (this->tuning_params_.ki * this->i_err_);
  double derivative = (this->tuning_params_.kd * d_err);
  double output = proportional + integral + derivative;
  double clamped_output = max(0.0, min(output / 100, 1.0));

  ESP_LOGI(TAG, "PID calculation:"); 
  ESP_LOGI(TAG, "  Input: %lf", this->current_temperature); 
  ESP_LOGI(TAG, "  Prev Input: %lf", this->previous_temperature_); 
  ESP_LOGI(TAG, "  Setpoint: %lf", this->target_temperature); 
  ESP_LOGI(TAG, "  Error: %lf", error); 
  ESP_LOGI(TAG, "  Rate of change: %lf", d_err); 
  ESP_LOGI(TAG, "  -------------------------"); 
  ESP_LOGI(TAG, "  Proportional: %lf", proportional); 
  ESP_LOGI(TAG, "  Integral: %lf", integral); 
  ESP_LOGI(TAG, "  Derivative: %lf", derivative); 
  ESP_LOGI(TAG, "  -------------------------"); 
  ESP_LOGI(TAG, "  Output: %lf", output); 
  ESP_LOGI(TAG, "  Output (clamped): %lf", clamped_output); 

  this->output = output;
  this->output_p = proportional;
  this->output_i = integral;
  this->output_d = derivative;

  this->previous_temperature_ = this->current_temperature;
  this->float_output_->set_level(clamped_output);
  this->switch_to_action_(clamped_output > 0 
      ? climate::ClimateAction::CLIMATE_ACTION_HEATING
      : climate::ClimateAction::CLIMATE_ACTION_OFF);
}


void PidClimate::switch_to_mode_(climate::ClimateMode mode) {
  if (mode == this->mode) {
    // already in target mode
    return;
  }

  this->mode = mode;
  switch(mode) {
    case climate::ClimateMode::CLIMATE_MODE_HEAT:
      this->switch_to_action_(climate::ClimateAction::CLIMATE_ACTION_HEATING);
      this->float_output_->turn_on();
      break;
    case climate::ClimateMode::CLIMATE_MODE_OFF:
      this->switch_to_action_(climate::ClimateAction::CLIMATE_ACTION_OFF);
      this->float_output_->turn_off();
      break;
    case climate::ClimateMode::CLIMATE_MODE_AUTO:
      break;
    default:
      ESP_LOGE(TAG, "Attempt to switch to unhandled mode: %d", mode);
      break;
  }
}

void PidClimate::switch_to_action_(climate::ClimateAction action) {
  if (action == this->action) {
    // already in target action
    return;
  }

  this->action = action;
  this->publish_state();
}

climate::ClimateTraits PidClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_supports_auto_mode(true);
  traits.set_supports_heat_mode(true);
  traits.set_supports_action(true);
  return traits;
}

void PidClimate::dump_config() {
   LOG_CLIMATE("", "PID Climate", this);
   LOG_UPDATE_INTERVAL(this);
   ESP_LOGCONFIG(TAG, "  kP: %lf", this->tuning_params_.kp);
   ESP_LOGCONFIG(TAG, "  kI: %lf", this->tuning_params_.ki);
   ESP_LOGCONFIG(TAG, "  kD: %lf", this->tuning_params_.kd);
   ESP_LOGCONFIG(TAG, "  I Max: %lf", this->tuning_params_.i_max);
   ESP_LOGCONFIG(TAG, "  I Enable: %lf", this->tuning_params_.i_enable);

}

PidClimate::PidClimate() : PollingComponent(1000) {}

PidClimateTuningParams::PidClimateTuningParams() = default;
PidClimateTuningParams::PidClimateTuningParams(float kp, float ki, float kd, float i_max, float i_enable)
    : kp(kp), ki(ki), kd(kd), i_max(i_max), i_enable(i_enable) {}

}  // namespace pid
}  // namespace esphome
