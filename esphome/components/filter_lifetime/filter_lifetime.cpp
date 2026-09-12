#include "filter_lifetime.h"
#include "esphome/core/log.h"

namespace esphome::filter_lifetime {
static const char *const TAG = "filter_lifetime";

void FilterLifetime::reset_filter() {
  this->runtime_minutes_ = 0.0f;
  this->pref_.save(&this->runtime_minutes_);
  this->publish_state(100.0f);
  if (this->runtime_hours_sensor_ != nullptr) {
    this->runtime_hours_sensor_->publish_state(0.0f);
  }
  if (this->remaining_days_sensor_ != nullptr) {
    this->remaining_days_sensor_->publish_state(this->max_lifetime_minutes_ / (24.0f * 60.0f));
  }
  ESP_LOGI(TAG, "Filter lifetime reset");
}

void FilterLifetime::dump_config() {
  LOG_SENSOR("", "Filter Lifetime", this);
  ESP_LOGCONFIG(TAG, "  Max Lifetime: %" PRIu32 " minutes (%.1f days)", this->max_lifetime_minutes_,
                this->max_lifetime_minutes_ / (24.0f * 60.0f));
  ESP_LOGCONFIG(TAG, "  Stored Runtime: %.1f minutes", this->runtime_minutes_);
  if (this->is_on_sensor_ != nullptr) {
    LOG_BINARY_SENSOR("  ", "Is-On Sensor", this->is_on_sensor_);
  } else if (this->is_on_lambda_) {
    ESP_LOGCONFIG(TAG, "  Is-On: lambda");
  } else {
    ESP_LOGCONFIG(TAG, "  Is-On: always on (default)");
  }
  if (this->current_speed_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Speed Sensor", this->current_speed_sensor_);
  } else if (this->current_speed_lambda_) {
    ESP_LOGCONFIG(TAG, "  Speed: lambda");
  } else {
    ESP_LOGCONFIG(TAG, "  Speed: 100%% (default)");
  }
  LOG_UPDATE_INTERVAL(this);
}

void FilterLifetime::setup() {
  this->last_update_ = App.get_loop_component_start_time();
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  this->pref_.load(&this->runtime_minutes_);
  if (std::isnan(this->runtime_minutes_) || std::isinf(this->runtime_minutes_) || this->runtime_minutes_ < 0.0f) {
    this->runtime_minutes_ = 0.0f;
  }
}

bool FilterLifetime::get_is_on_() {
  // is_on_sensor_ state defaults to false before first publish, which conservatively
  // skips accumulation until the sensor reports — acceptable at boot.
  if (this->is_on_sensor_ != nullptr) {
    return this->is_on_sensor_->state;
  }
  if (this->is_on_lambda_) {
    return this->is_on_lambda_();
  }
  return true;
}

float FilterLifetime::get_current_speed_() {
  if (this->current_speed_sensor_ != nullptr) {
    // NaN before first publish: return 0 to skip accumulation rather than assume 100%.
    return std::isnan(this->current_speed_sensor_->state) ? 0.0f : this->current_speed_sensor_->state;
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

  bool is_on = this->get_is_on_();
  float speed = this->get_current_speed_();

  if (std::isnan(speed)) {
    speed = 0.0f;
  }
  speed = std::max(0.0f, std::min(100.0f, speed));

  ESP_LOGV(TAG, "Update: elapsed=%.2f min, is_on=%d, speed=%.1f%%", elapsed_minutes, is_on, speed);

  if (is_on) {
    float added = elapsed_minutes * (speed / 100.0f);
    uint32_t prev_whole_minutes = static_cast<uint32_t>(this->runtime_minutes_);
    this->runtime_minutes_ += added;
    // Save only when the whole-minute count changes to limit flash wear.
    if (static_cast<uint32_t>(this->runtime_minutes_) != prev_whole_minutes) {
      this->pref_.save(&this->runtime_minutes_);
    }
    ESP_LOGV(TAG, "Added %.2f minutes, total runtime=%.2f", added, this->runtime_minutes_);
  }

  float max_minutes = static_cast<float>(this->max_lifetime_minutes_);
  float lifetime_pct = 100.0f;
  if (max_minutes > 0.0f) {
    lifetime_pct = 100.0f * std::max(0.0f, 1.0f - (this->runtime_minutes_ / max_minutes));
  }

  ESP_LOGD(TAG, "Runtime: %.1f/%.1f min, Remaining: %.1f%%", this->runtime_minutes_, max_minutes, lifetime_pct);
  this->publish_state(lifetime_pct);

  if (this->runtime_hours_sensor_ != nullptr) {
    this->runtime_hours_sensor_->publish_state(this->runtime_minutes_ / 60.0f);
  }

  if (this->remaining_days_sensor_ != nullptr) {
    float remaining_days = (max_minutes - this->runtime_minutes_) / (24.0f * 60.0f);
    this->remaining_days_sensor_->publish_state(std::max(0.0f, remaining_days));
  }
}

}  // namespace esphome::filter_lifetime
