#include "filter_lifetime.h"

namespace esphome {
namespace filter_lifetime {
static const char *const TAG = "filter.lifetime";
void FilterLifetime::reset_filter() {
  this->runtime_minutes_ = 0.0f;
  this->pref_.save(&this->runtime_minutes_);
  this->publish_state(100.0f);
}

void FilterLifetime::dump_config() { ESP_LOGCONFIG(TAG, "Stored runtime_minutes: %f", this->runtime_minutes_); }

void FilterLifetime::setup() {
  this->last_update_ = App.get_loop_component_start_time();
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  this->pref_.load(&this->runtime_minutes_);
  // PollingComponent handles update interval
}

void FilterLifetime::set_max_lifetime(int max_lifetime) { this->max_lifetime_ = max_lifetime; }

void FilterLifetime::set_is_on(std::function<bool()> is_on) { this->is_on_ = is_on; }

void FilterLifetime::set_current_speed(std::function<float()> current_speed) { this->current_speed_ = current_speed; }

void FilterLifetime::update() {
  uint32_t now = App.get_loop_component_start_time();
  float elapsed_minutes = (now - this->last_update_) / 60000.0f;  // ms to minutes
  this->last_update_ = now;
  bool is_on = this->is_on_ ? this->is_on_() : true;
  float speed = this->current_speed_ ? this->current_speed_() : 100.0f;
  ESP_LOGV(TAG, "Update: elapsed_minutes=%.2f, is_on=%d, speed=%.2f", elapsed_minutes, is_on, speed);
  if (is_on) {
    float added = elapsed_minutes * (speed / 100.0f);
    this->runtime_minutes_ += added;
    this->pref_.save(&this->runtime_minutes_);
    ESP_LOGV(TAG, "Added %.2f minutes, new runtime_minutes=%.2f", added, this->runtime_minutes_);
  }
  float max_minutes = this->max_lifetime_ * 30.4375f * 24.0f * 60.0f;  // months to minutes (avg month)
  float lifetime_pct = 100.0f;
  if (max_minutes > 0.0f) {
    lifetime_pct = 100.0f * std::max(0.0f, 1.0f - (this->runtime_minutes_ / max_minutes));
    lifetime_pct = std::floor(lifetime_pct * 100.0f) / 100.0f;
  }
  ESP_LOGV(TAG, "max_minutes=%.2f, lifetime_pct=%.2f", max_minutes, lifetime_pct);
  this->publish_state(lifetime_pct);
}

}  // namespace filter_lifetime
}  // namespace esphome
