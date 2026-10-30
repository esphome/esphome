#include "filter_lifetime.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::filter_lifetime {

ESPHOME_LOG_TAG(TAG, "filter_lifetime");

static constexpr uint32_t SECONDS_PER_HOUR = 3600;
static constexpr float MINUTES_PER_DAY = 24.0f * 60.0f;

void FilterLifetime::setup() {
  this->pref_ = this->make_entity_preference<uint32_t>();
  this->pref_.load(&this->runtime_seconds_);
  this->last_update_ = millis();
  this->publish_values_();
}

void FilterLifetime::update() {
  const uint32_t now = App.get_loop_component_start_time();
  const uint32_t elapsed_ms = now - this->last_update_;
  this->last_update_ = now;

  if (this->get_is_on_()) {
    float speed = this->get_current_speed_();
    if (std::isnan(speed))
      speed = 0.0f;
    speed = std::clamp(speed, 0.0f, 100.0f);
    const uint32_t prev_hours = this->runtime_seconds_ / SECONDS_PER_HOUR;
    this->carry_ms_ += static_cast<uint32_t>(elapsed_ms * (speed / 100.0f));
    this->runtime_seconds_ += this->carry_ms_ / 1000;
    this->carry_ms_ %= 1000;
    // Save once per hour of runtime (and in on_shutdown): every save erases a flash sector on ESP8266.
    if (this->runtime_seconds_ / SECONDS_PER_HOUR != prev_hours)
      this->pref_.save(&this->runtime_seconds_);
    ESP_LOGV(TAG, "Elapsed %" PRIu32 " ms at %.0f%%, runtime %" PRIu32 " s", elapsed_ms, speed, this->runtime_seconds_);
  }
  this->publish_values_();
}

void FilterLifetime::publish_values_() {
  const float runtime_minutes = this->runtime_seconds_ / 60.0f;
  const float max_minutes = this->max_lifetime_minutes_;
  const float remaining_minutes = std::max(0.0f, max_minutes - runtime_minutes);
  this->publish_state(100.0f * remaining_minutes / max_minutes);
  PUBLISH_STATE(this->runtime_hours_sensor_, runtime_minutes / 60.0f);
  PUBLISH_STATE(this->remaining_days_sensor_, remaining_minutes / MINUTES_PER_DAY);
}

void FilterLifetime::on_shutdown() { this->pref_.save(&this->runtime_seconds_); }

void FilterLifetime::reset_filter() {
  this->runtime_seconds_ = 0;
  this->carry_ms_ = 0;
  this->pref_.save(&this->runtime_seconds_);
  this->publish_values_();
  ESP_LOGI(TAG, "Filter lifetime reset");
}

bool FilterLifetime::get_is_on_() {
#ifdef USE_BINARY_SENSOR
  // Reads false until the sensor first publishes, so nothing accumulates before then.
  if (this->is_on_binary_sensor_ != nullptr)
    return this->is_on_binary_sensor_->state;
#endif
  if (this->is_on_lambda_ != nullptr)
    return this->is_on_lambda_();
  return true;
}

float FilterLifetime::get_current_speed_() {
  if (this->current_speed_sensor_ != nullptr)
    return this->current_speed_sensor_->state;  // NaN before the first publish, treated as 0
  if (this->current_speed_lambda_ != nullptr)
    return this->current_speed_lambda_();
  return 100.0f;
}

void FilterLifetime::dump_config() {
  LOG_SENSOR("", "Filter Lifetime", this);
  ESP_LOGCONFIG(TAG,
                "  Max lifetime: %" PRIu32 " min (%.1f days)\n"
                "  Runtime: %.1f h",
                this->max_lifetime_minutes_, this->max_lifetime_minutes_ / MINUTES_PER_DAY,
                this->runtime_seconds_ / 3600.0f);
#ifdef USE_BINARY_SENSOR
  if (this->is_on_binary_sensor_ != nullptr) {
    LOG_BINARY_SENSOR("  ", "Is-on sensor", this->is_on_binary_sensor_);
  } else
#endif
  {
    ESP_LOGCONFIG(TAG, "  Is-on: %s",
                  this->is_on_lambda_ != nullptr ? LOG_STR_LITERAL("lambda") : LOG_STR_LITERAL("always"));
  }
  if (this->current_speed_sensor_ != nullptr) {
    LOG_SENSOR("  ", "Speed sensor", this->current_speed_sensor_);
  } else {
    ESP_LOGCONFIG(TAG, "  Speed: %s",
                  this->current_speed_lambda_ != nullptr ? LOG_STR_LITERAL("lambda") : LOG_STR_LITERAL("100%"));
  }
  LOG_SENSOR("  ", "Runtime hours", this->runtime_hours_sensor_);
  LOG_SENSOR("  ", "Remaining days", this->remaining_days_sensor_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::filter_lifetime
