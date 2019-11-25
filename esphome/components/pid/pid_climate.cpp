#include "pid_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pid {

static const char *TAG = "pid.climate";

void PidClimate::setup() {
  this->sensor_->add_on_state_callback([this](float state) {
    this->current_temperature = state;
    // control may have changed, recompute
    // this->compute_state_();
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
  // if (call.get_mode().has_value())
  //   this->mode = *call.get_mode();
  // if (call.get_target_temperature_low().has_value())
  //   this->target_temperature_low = *call.get_target_temperature_low();
  // if (call.get_target_temperature_high().has_value())
  //   this->target_temperature_high = *call.get_target_temperature_high();
  // if (call.get_away().has_value())
  //   this->change_away_(*call.get_away());

  // this->compute_state_();
  this->publish_state();
}

void PidClimate::update() {
  ESP_LOGD("custom", "Update");
}


climate::ClimateTraits PidClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_supports_auto_mode(true);
  traits.set_supports_cool_mode(this->supports_cool_);
  traits.set_supports_heat_mode(this->supports_heat_);
  // traits.set_supports_two_point_target_temperature(true);
  traits.set_supports_away(false);
  traits.set_supports_action(true);
  return traits;
}
PidClimate::PidClimate()
    : PollingComponent(15000), idle_trigger_(new Trigger<>()), cool_trigger_(new Trigger<>()), heat_trigger_(new Trigger<>()) {}
void PidClimate::set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
Trigger<> *PidClimate::get_idle_trigger() const { return this->idle_trigger_; }
Trigger<> *PidClimate::get_cool_trigger() const { return this->cool_trigger_; }
void PidClimate::set_supports_cool(bool supports_cool) { this->supports_cool_ = supports_cool; }
Trigger<> *PidClimate::get_heat_trigger() const { return this->heat_trigger_; }
void PidClimate::set_supports_heat(bool supports_heat) { this->supports_heat_ = supports_heat; }
void PidClimate::set_target_temperature(float target_temperature ) { this->target_temperature = target_temperature; }

}  // namespace pid
}  // namespace esphome
