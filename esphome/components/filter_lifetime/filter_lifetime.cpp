#include "filter_lifetime.h"
#include "esphome/core/log.h"

namespace esphome {
namespace filter_lifetime {
static const char *const TAG = "filter_lifetime";

void FilterLifetime::reset_filter() {
  this->runtime_minutes_ = 0.0f;
  this->pref_.save(&this->runtime_minutes_);
  this->publish_state(100.0f);
  ESP_LOGI(TAG, "Filter lifetime reset");
}

void FilterLifetime::dump_config() {
  LOG_SENSOR("", "Filter Lifetime", this);
  ESP_LOGCONFIG(TAG, "  Max Lifetime: %d months", this->max_lifetime_);
  ESP_LOGCONFIG(TAG, "  Stored Runtime: %.1f minutes", this->runtime_minutes_);
}

void FilterLifetime::setup() {
  this->last_update_ = App.get_loop_component_start_time();
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  this->pref_.load(&this->runtime_minutes_);
}

bool FilterLifetime::get_is_on_() {
  // Check binary sensor first, then lambda, default to true
  if (this->is_on_sensor_ != nullptr) {
    return this->is_on_sensor_->state;
  }
  if (this->is_on_lambda_) {
    return this->is_on_lambda_();
  }
  return true;
}

float FilterLifetime::get_current_speed_() {
  // Check sensor first, then lambda, default to 100.0
  if (this->current_speed_sensor_ != nullptr) {
    if (!std::isnan(this->current_speed_sensor_->state)) {
      return this->current_speed_sensor_->state;
    }
  }
  if (this->current_speed_lambda_) {
    return this->current_speed_lambda_();
  }
  return 100.0f;
}

void FilterLifetime::update() {
  uint32_t now = App.get_loop_component_start_time();
  float elapsed_minutes = (now - this->last_update_) / 60000.0f;  // Convert ms to minutes
  this->last_update_ = now;

  // Get device state
  bool is_on = this->get_is_on_();
  float speed = this->get_current_speed_();

  ESP_LOGV(TAG, "Update: elapsed=%.2f min, is_on=%d, speed=%.1f%%", elapsed_minutes, is_on, speed);

  // Add runtime if device is on, scaled by speed percentage
  if (is_on) {
    float added = elapsed_minutes * (speed / 100.0f);
    this->runtime_minutes_ += added;
    this->pref_.save(&this->runtime_minutes_);
    ESP_LOGV(TAG, "Added %.2f minutes, total runtime=%.2f", added, this->runtime_minutes_);
  }

  // Calculate remaining lifetime percentage
  float max_minutes = this->max_lifetime_ * 30.4375f * 24.0f * 60.0f;  // Average days per month
  float lifetime_pct = 100.0f;
  if (max_minutes > 0.0f) {
    lifetime_pct = 100.0f * std::max(0.0f, 1.0f - (this->runtime_minutes_ / max_minutes));
  }

  ESP_LOGD(TAG, "Runtime: %.1f/%.1f min, Remaining: %.1f%%", this->runtime_minutes_, max_minutes, lifetime_pct);
  this->publish_state(lifetime_pct);

  // Update optional sensors
  if (this->runtime_hours_sensor_ != nullptr) {
    float runtime_hours = this->runtime_minutes_ / 60.0f;
    this->runtime_hours_sensor_->publish_state(runtime_hours);
  }

  if (this->remaining_days_sensor_ != nullptr) {
    float remaining_minutes = max_minutes - this->runtime_minutes_;
    float remaining_days = remaining_minutes / (24.0f * 60.0f);
    this->remaining_days_sensor_->publish_state(std::max(0.0f, remaining_days));
  }
}

}  // namespace filter_lifetime
}  // namespace esphome
