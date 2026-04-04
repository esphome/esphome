#include "total_daily_energy.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::total_daily_energy {

static const char *const TAG = "total_daily_energy";
static constexpr uint32_t MIDNIGHT_TIMEOUT = 1;
static constexpr uint8_t SECONDS_PER_MINUTE = 60;
static constexpr uint8_t MINUTES_PER_HOUR = 60;
static constexpr uint8_t HOURS_PER_DAY = 24;
static constexpr uint32_t SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR;
static constexpr uint16_t MS_PER_SECOND = 1000;

void TotalDailyEnergy::setup() {
  float initial_value = 0;

  if (this->restore_) {
    this->pref_ = this->make_entity_preference<float>();
    this->pref_.load(&initial_value);
  }
  this->publish_state_and_save(initial_value);

  this->last_update_ = App.get_loop_component_start_time();

  this->parent_->add_on_state_callback([this](float state) { this->process_new_state_(state); });

  // Schedule initial midnight reset if time is already valid, otherwise
  // the time sync callback will handle it once time becomes available.
  this->schedule_midnight_reset_();
  // Re-schedule on every NTP sync in case the clock jumped across midnight.
  // DST transitions don't trigger this callback (DST is a local time interpretation,
  // not an epoch change), but DST is handled correctly because set_timeout uses real
  // elapsed time (millis) and schedule_midnight_reset_ re-reads now() when it fires.
  this->time_->add_on_time_sync_callback([this]() { this->schedule_midnight_reset_(); });
}

void TotalDailyEnergy::dump_config() { LOG_SENSOR("", "Total Daily Energy", this); }

void TotalDailyEnergy::schedule_midnight_reset_() {
  auto t = this->time_->now();
  if (!t.is_valid())
    return;

  // Check if the day changed (time sync moved us past midnight, or first call)
  if (this->last_day_of_year_ != t.day_of_year) {
    if (this->last_day_of_year_ != 0) {
      // Day actually changed — reset energy
      this->total_energy_ = 0;
      this->publish_state_and_save(0);
    }
    this->last_day_of_year_ = t.day_of_year;
  }

  // Calculate seconds until next midnight (+ 1s buffer to ensure we're past midnight).
  // Uses the same MIDNIGHT_TIMEOUT ID so re-scheduling (e.g. from time sync) cancels
  // any previously pending timeout.
  uint32_t seconds_until_midnight =
      ((HOURS_PER_DAY - 1 - t.hour) * MINUTES_PER_HOUR + (MINUTES_PER_HOUR - 1 - t.minute)) * SECONDS_PER_MINUTE +
      (SECONDS_PER_MINUTE - t.second) + 1;

  ESP_LOGD(TAG, "Scheduling midnight reset in %us", seconds_until_midnight);
  this->set_timeout(MIDNIGHT_TIMEOUT, seconds_until_midnight * MS_PER_SECOND,
                    [this]() { this->schedule_midnight_reset_(); });
}

void TotalDailyEnergy::publish_state_and_save(float state) {
  this->total_energy_ = state;
  this->publish_state(state);
  if (this->restore_) {
    this->pref_.save(&state);
  }
}

void TotalDailyEnergy::process_new_state_(float state) {
  if (std::isnan(state))
    return;
  const uint32_t now = App.get_loop_component_start_time();
  const float old_state = this->last_power_state_;
  const float new_state = state;
  float delta_hours = (now - this->last_update_) / static_cast<float>(MS_PER_SECOND) / SECONDS_PER_HOUR;
  float delta_energy = 0.0f;
  switch (this->method_) {
    case TOTAL_DAILY_ENERGY_METHOD_TRAPEZOID:
      delta_energy = delta_hours * (old_state + new_state) / 2.0f;
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

}  // namespace esphome::total_daily_energy
