#include "total_daily_energy.h"
#include "esphome/core/log.h"

namespace esphome {
namespace total_daily_energy {

static const char *const TAG = "total_daily_energy";

void TotalDailyEnergy::setup() {
  float initial_value = 0;

  if (this->restore_) {
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    this->pref_.load(&initial_value);
  }
  this->publish_state_and_save(initial_value);

  this->last_update_ = millis();
  this->last_save_ = this->last_update_;

  this->parent_->add_on_state_callback([this](float state) { this->process_new_state_(state); });
}

void TotalDailyEnergy::dump_config() { LOG_SENSOR("", "Total Daily Energy", this); }

void TotalDailyEnergy::loop() {
  auto t = this->time_->now();
  if (!t.is_valid())
    return;

  if (this->last_day_of_year_ == 0) {
    this->last_day_of_year_ = t.day_of_year;
    return;
  }

  if (t.day_of_year != this->last_day_of_year_) {
    this->last_day_of_year_ = t.day_of_year;
    this->total_energy_ = 0;
    this->publish_state_and_save(0);
  }
}

void TotalDailyEnergy::publish_state_and_save(float state) {
  this->total_energy_ = state;
  this->publish_state(state);
  const uint32_t now = millis();
  if (now - this->last_save_ < this->min_save_interval_) {
    return;
  }
  this->last_save_ = now;
  this->pref_.save(&state);
}

void TotalDailyEnergy::process_new_state_(float state) {
  if (std::isnan(state))
    return;
  const uint32_t now = millis();
  const float old_state = this->last_power_state_;
  const float new_state = state;
  float delta_hours = (now - this->last_update_) / 1000.0f / 60.0f / 60.0f;
  float delta_energy = 0.0f;
  switch (this->method_) {
    case TOTAL_DAILY_ENERGY_METHOD_TRAPEZOID:
      delta_energy = delta_hours * (old_state + new_state) / 2.0;
      break;
    case TOTAL_DAILY_ENERGY_METHOD_LEFT:
      delta_energy = delta_hours * old_state;
      break;
    case TOTAL_DAILY_ENERGY_METHOD_RIGHT:
      delta_energy = delta_hours * new_state;
      break;
  }
  this->last_power_state_ = new_state;
  this->last_update_ = now;
  this->publish_state_and_save(this->total_energy_ + delta_energy);
}

}  // namespace total_daily_energy
}  // namespace esphome
