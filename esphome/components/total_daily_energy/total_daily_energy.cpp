#include "total_daily_energy.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::total_daily_energy {

static const char *const TAG = "total_daily_energy";
static constexpr uint32_t TIMEOUT_ID_MIDNIGHT = 1;
static constexpr uint8_t SECONDS_PER_MINUTE = 60;
static constexpr uint8_t MINUTES_PER_HOUR = 60;
static constexpr uint8_t HOURS_PER_DAY = 24;
static constexpr uint32_t SECONDS_PER_HOUR = SECONDS_PER_MINUTE * MINUTES_PER_HOUR;
static constexpr uint16_t MILLIS_PER_SECOND = 1000;
// Wake up 90 minutes before midnight to recalculate, ensuring DST transitions
// (which shift wall clock by 1 hour but don't change millis()) don't cause
// the midnight reset to fire late. DST transitions don't trigger the time sync
// callback since they change local time interpretation, not the epoch.
static constexpr uint32_t PRE_MIDNIGHT_SECONDS = 90 * SECONDS_PER_MINUTE;

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

  // Calculate seconds until next midnight.
  // Uses the same TIMEOUT_ID_MIDNIGHT ID so re-scheduling (e.g. from time sync) cancels
  // any previously pending timeout.
  uint32_t seconds_until_midnight =
      ((HOURS_PER_DAY - 1 - t.hour) * MINUTES_PER_HOUR + (MINUTES_PER_HOUR - 1 - t.minute)) * SECONDS_PER_MINUTE +
      (SECONDS_PER_MINUTE - t.second);

  // set_timeout counts real elapsed millis, but DST shifts wall clock by up to 1 hour
  // without changing millis. To avoid firing up to 1 hour late/early, we use two stages:
  // 1) Wake up 90 minutes before midnight to recalculate with current wall clock
  // 2) From there, schedule the precise midnight reset
  uint32_t timeout_seconds;
  if (seconds_until_midnight > PRE_MIDNIGHT_SECONDS) {
    timeout_seconds = seconds_until_midnight - PRE_MIDNIGHT_SECONDS;
  } else {
    timeout_seconds = seconds_until_midnight + 1;
  }

  ESP_LOGD(TAG, "Scheduling midnight check in %us", timeout_seconds);
  this->set_timeout(TIMEOUT_ID_MIDNIGHT, timeout_seconds * MILLIS_PER_SECOND,
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
  float delta_hours = (now - this->last_update_) / static_cast<float>(MILLIS_PER_SECOND) / SECONDS_PER_HOUR;
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
