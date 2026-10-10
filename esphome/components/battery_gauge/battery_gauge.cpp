#include "battery_gauge.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cmath>
#include <algorithm>

namespace esphome::battery_gauge {

static const char *const TAG = "battery_gauge";

// Time constant for the current/voltage EMA filters. Hardcoded for now; may become configurable
// once a real need for tuning shows up.
static constexpr float FILTER_TIME_CONSTANT_S = 30.0f;

// Preference-write hysteresis: only persist the charge percentage when it has moved by at least
// this many tenths-of-a-percent, or when this much time has passed since the last write,
// whichever comes first. Bounds flash wear from writing on every sample.
static constexpr unsigned PREFERENCE_SAVE_MIN_DELTA_PERMILLE = 5;
static constexpr uint32_t PREFERENCE_SAVE_MIN_INTERVAL_MS = 5 * 60 * 1000;

float BatteryGauge::ema_update(float filtered, float raw, float dt_s, float tau_s) {
  float alpha = 1.0f - std::exp(-dt_s / tau_s);
  return filtered + alpha * (raw - filtered);
}

void BatteryGauge::on_current_(float value) {
  if (!std::isfinite(value))
    return;
  auto current = value;
  if (std::isfinite(this->last_current_)) {
    current += this->last_current_;
    current /= 2.0f;
  }
  this->last_current_ = value;

  auto now = millis();
  auto previous = this->last_time_;
  this->last_time_ = now;
  float dt_s = (now - previous) / 1000.0f;

  if (!std::isfinite(this->filtered_current_)) {
    this->filtered_current_ = value;
  } else {
    this->filtered_current_ = ema_update(this->filtered_current_, value, dt_s, FILTER_TIME_CONSTANT_S);
  }

  if (previous == 0)
    return;

  // Apply chemistry-specific derating: coulombic efficiency (+ SoC-dependent acceptance) on
  // charge currents, rate-dependent capacity loss (e.g. Peukert) on discharge currents.
  auto soc = this->charge_state_ / this->capacity_;
  if (current > 0) {
    current *= this->efficiency_ * this->chemistry_->charge_acceptance(soc);
  } else if (current < 0) {
    current *= this->chemistry_->discharge_scale(current);
  }
  float interval_hours = dt_s / 3600.0f;
  auto delta = current * interval_hours;
  ESP_LOGV(TAG, "current: %f, interval: %f, delta: %f, charge state: %f", current, interval_hours, delta,
           this->charge_state_);
  this->publish_(this->charge_state_ + delta);
}

// Publish new charge state, in Ah
void BatteryGauge::publish_(float new_state) {
  this->charge_state_ = std::max(0.0f, std::min(new_state, this->capacity_));
  auto percentage = this->charge_state_ / this->capacity_ * 100.0f;
  if (this->state_of_charge_sensor_ != nullptr)
    this->state_of_charge_sensor_->publish_state(percentage);

  unsigned new_percentage = std::lround(percentage * 10.0f);
  if (new_percentage == this->persisted_.percentage_x10)
    return;

  unsigned delta = new_percentage > this->persisted_.percentage_x10 ? new_percentage - this->persisted_.percentage_x10
                                                                    : this->persisted_.percentage_x10 - new_percentage;
  auto now = millis();
  bool save_due = now - this->last_saved_time_ >= PREFERENCE_SAVE_MIN_INTERVAL_MS;
  if (delta < PREFERENCE_SAVE_MIN_DELTA_PERMILLE && !save_due)
    return;

  this->persisted_.percentage_x10 = new_percentage;
  this->last_saved_time_ = now;
  ESP_LOGV(TAG, "Saving charge percentage: %u", this->persisted_.percentage_x10);
  this->saved_state_.save(&this->persisted_);
}

// Using voltage thresholds to adjust the charge state. This resynchronises at full charge
void BatteryGauge::on_voltage_(float value) {
  if (!std::isfinite(value))
    return;

  auto now = millis();
  auto previous = this->last_voltage_time_;
  this->last_voltage_time_ = now;
  float dt_s = (now - previous) / 1000.0f;

  if (!std::isfinite(this->filtered_voltage_)) {
    this->filtered_voltage_ = value;
    return;
  }
  this->filtered_voltage_ = ema_update(this->filtered_voltage_, value, dt_s, FILTER_TIME_CONSTANT_S);

  GaugeState state{
      .filtered_voltage = this->filtered_voltage_,
      .filtered_current = this->filtered_current_,
      .charge_state = this->charge_state_,
      .capacity = this->capacity_,
      .max_charge_voltage = this->max_charge_voltage_,
  };

  if (!this->chemistry_->is_full(state)) {
    this->full_condition_since_ = 0;
    return;
  }
  if (this->full_condition_since_ == 0)
    this->full_condition_since_ = now;
  if (now - this->full_condition_since_ < this->chemistry_->full_charge_dwell_ms())
    return;
  if (this->charge_state_ != this->capacity_) {
    this->publish_(this->capacity_);
    ESP_LOGD(TAG, "Charging: Voltage %f, state set to 100%%", this->filtered_voltage_);
  }
}

unsigned BatteryGauge::fraction_to_percentage_x10(float fraction) { return std::lround(fraction * 1000.0f); }

float BatteryGauge::percentage_x10_to_state(unsigned percentage_x10, float capacity) {
  return percentage_x10 / 1000.0f * capacity;
}

void BatteryGauge::setup() {
  this->saved_state_ = global_preferences->make_preference<PersistedState>(this->pref_key_);
  this->current_source_->add_on_state_callback([this](float value) { this->on_current_(value); });
  this->voltage_source_->add_on_state_callback([this](float value) { this->on_voltage_(value); });
  if (!this->saved_state_.load(&this->persisted_)) {
    ESP_LOGD(TAG, "Setting initial charge state to %f", this->initial_state_);
    this->persisted_.percentage_x10 = fraction_to_percentage_x10(this->initial_state_);
    this->saved_state_.save(&this->persisted_);
  }
  this->charge_state_ = percentage_x10_to_state(this->persisted_.percentage_x10, this->capacity_);
  if (this->state_of_charge_sensor_ != nullptr)
    this->state_of_charge_sensor_->publish_state(this->charge_state_ / this->capacity_ * 100.0f);
}

void BatteryGauge::dump_config() {
  ESP_LOGCONFIG(TAG, "Battery Gauge:");
  if (this->capacity_ < 10.0f) {
    ESP_LOGCONFIG(TAG, "  Capacity: %.0fmAh", this->capacity_ * 1000.0);
  } else {
    ESP_LOGCONFIG(TAG, "  Capacity: %.1fAh", this->capacity_);
  }
  ESP_LOGCONFIG(TAG, "  Max charge voltage: %.1f", this->max_charge_voltage_);
  PersistedState saved{};
  if (this->saved_state_.load(&saved)) {
    ESP_LOGCONFIG(TAG, "  Saved charge percentage: %.1f", saved.percentage_x10 / 10.0f);
  }
  LOG_SENSOR("  ", "State of Charge", this->state_of_charge_sensor_);
}

}  // namespace esphome::battery_gauge
