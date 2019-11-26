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
    // restore from defaults, change_away handles those for us
    this->mode = climate::CLIMATE_MODE_AUTO;
  }
}

void PidClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value())
    this->set_mode_(*call.get_mode());
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();

  this->compute_state_();
  this->publish_state();
}

void PidClimate::set_mode_(climate::ClimateMode new_mode) {
  if (new_mode == this->mode) {
    // Mode has not changed
    return;
  }

  this->mode = new_mode;
  if (new_mode != climate::CLIMATE_MODE_AUTO) {
    // Mode is no longer AUTO, disable the PID controller.
    this->pid_controller_->SetMode(MANUAL);
  }
}

void PidClimate::loop() {
  this->input_ = this->current_temperature;
  this->setpoint_ = this->target_temperature;

  if (isnan(this->input_) || this->mode != climate::CLIMATE_MODE_AUTO) {
    // Current temperature reading is NaN or mode is not AUTO. No need to 
    // perform any calculations
    return;
  }
  
  if (this->pid_controller_->GetMode() != AUTOMATIC) {
    // PID controller has not been initialized for use. Initialize it now.
    ESP_LOGI(TAG, "Setting PID controller to AUTOMATIC mode");
    this->pid_controller_->SetSampleTime(this->pid_config_.sample_time);
    this->pid_controller_->SetOutputLimits(-100, 100);
    this->pid_controller_->SetTunings(this->pid_config_.kp, this->pid_config_.ki, this->pid_config_.kd);
    this->pid_controller_->SetMode(AUTOMATIC);
  }
  
  if(this->pid_controller_->Compute()) {
    // A PID computation was made. Re-compute the state.
    this->log_state();
    this->compute_state_();
  }
}

void PidClimate::compute_state_() {
  if (this->mode != climate::CLIMATE_MODE_AUTO) {
    // in non-auto mode
    this->switch_to_action_(static_cast<climate::ClimateAction>(this->mode));
    return;
  }
  if (isnan(this->current_temperature) || isnan(this->target_temperature) || isnan(this->output_)) {
    // if any control values are nan, go to OFF (idle) mode
    this->switch_to_action_(climate::CLIMATE_ACTION_OFF);
    return;
  }
  
  const bool too_cold = this->output_ > 0;
  const bool too_hot = this->output_ <= 0;

  climate::ClimateAction target_action;
  if (too_cold) {
    // too cold -> enable heating if possible, else idle
    if (this->supports_heat_)
      target_action = climate::CLIMATE_ACTION_HEATING;
    else
      target_action = climate::CLIMATE_ACTION_OFF;
  } else if (too_hot) {
    // too hot -> enable cooling if possible, else idle
    if (this->supports_cool_)
      target_action = climate::CLIMATE_ACTION_COOLING;
    else
      target_action = climate::CLIMATE_ACTION_OFF;
  } else {
    // neither too hot nor too cold -> in range
    if (this->supports_cool_ && this->supports_heat_) {
      // if supports both ends, go to idle mode
      target_action = climate::CLIMATE_ACTION_OFF;
    } else {
      // else use current mode and don't change (hysteresis)
      target_action = this->action;
    }
  }

  this->switch_to_action_(target_action);
}

void PidClimate::switch_to_action_(climate::ClimateAction action) {
  if (action == this->action) {
    // already in target mode
    return;
  }
  
  if (this->prev_trigger_ != nullptr) {
    this->prev_trigger_->stop();
    this->prev_trigger_ = nullptr;
  }

  Trigger<> *trig;
  switch (action) {
    case climate::CLIMATE_ACTION_OFF:
      trig = this->idle_trigger_;
      break;
    case climate::CLIMATE_ACTION_COOLING:
      trig = this->cool_trigger_;
      break;
    case climate::CLIMATE_ACTION_HEATING:
      trig = this->heat_trigger_;
      break;
    default:
      trig = nullptr;
  }

  if (trig != nullptr) {
    // trig should never be null, but still check so that we don't crash
    trig->trigger();
    this->action = action;
    this->prev_trigger_ = trig;
    this->publish_state();
  }
}

void PidClimate::log_state() {
  ESP_LOGI(TAG, "Input: %lf", this->current_temperature);
  ESP_LOGI(TAG, "Setpoint: %lf", this->target_temperature);
  ESP_LOGI(TAG, "PID Output: %lf", this->output_);
}

climate::ClimateTraits PidClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_supports_auto_mode(true);
  traits.set_supports_cool_mode(this->supports_cool_);
  traits.set_supports_heat_mode(this->supports_heat_);
  traits.set_supports_action(true);
  return traits;
}

void PidClimate::set_pid_config(const PidClimatePidConfig &pid_config) {
  this->pid_config_ = pid_config;
}

PidClimate::PidClimate() : 
    Component(), 
    idle_trigger_(new Trigger<>()), 
    cool_trigger_(new Trigger<>()), 
    heat_trigger_(new Trigger<>()),
    pid_controller_(new PID(
      (double *)&this->input_, 
      &this->output_, 
      (double *)&this->setpoint_, 
      0, 0, 0, DIRECT)) {}

void PidClimate::set_sensor(sensor::Sensor *sensor) { 
  this->sensor_ = sensor; 
}

Trigger<> *PidClimate::get_idle_trigger() const { 
  return this->idle_trigger_; 
  }

Trigger<> *PidClimate::get_cool_trigger() const { 
  return this->cool_trigger_; 
}

void PidClimate::set_supports_cool(bool supports_cool) { 
  this->supports_cool_ = supports_cool;
}

Trigger<> *PidClimate::get_heat_trigger() const { 
  return this->heat_trigger_; 
}

void PidClimate::set_supports_heat(bool supports_heat) { 
  this->supports_heat_ = supports_heat; 
}

void PidClimate::set_target_temperature(float target_temperature ) { 
  this->target_temperature = target_temperature; 
}

PidClimatePidConfig::PidClimatePidConfig() = default;
PidClimatePidConfig::PidClimatePidConfig(unsigned int sample_time, float kp, float ki, float kd)
    : sample_time(sample_time), kp(kp), ki(ki), kd(kd) {}

}  // namespace pid
}  // namespace esphome
