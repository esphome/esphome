#include "servo.h"
#include "esphome/core/log.h"

namespace esphome {
namespace servo {

static const char *TAG = "servo";

uint32_t global_servo_id = 1911044085ULL;

void Servo::dump_config() {
  ESP_LOGCONFIG(TAG, "Servo:");
  ESP_LOGCONFIG(TAG, "  Idle Level: %.1f%%", this->idle_level_ * 100.0f);
  ESP_LOGCONFIG(TAG, "  Min Level: %.1f%%", this->min_level_ * 100.0f);
  ESP_LOGCONFIG(TAG, "  Max Level: %.1f%%", this->max_level_ * 100.0f);
  ESP_LOGCONFIG(TAG, "  keep on time: %d ms", this->keep_on_time_);
  ESP_LOGCONFIG(TAG, "  run duration: %d ms", this->run_duration_);
}

void Servo::loop() {
  // check if keep_on_time is reached and detach servo
  if (this->keep_on_time_ && this->start_millis_ && this->state_ == 0) {
    if (millis() > this->start_millis_ + this->keep_on_time_) {
      this->detach();
      this->start_millis_ = 0;
      ESP_LOGD(TAG, "Servo detached on keep_on_time");
    }
  }
  if (!this->run_duration_ && this->target_value_ != this->current_value_ && this->state_ == 1) {
    this->write_internal(this->target_value_);
  }
  if (this->run_duration_ && this->target_value_ != this->current_value_ && this->state_ == 1) {
    float new_value;
    float travel_diff = this->target_value_ - this->source_value_;
    long long target_runtime = abs((int) ((travel_diff) * this->run_duration_ * 1.0f / 2.0f));
    long long current_runtime = millis() - this->start_millis_;
    float percentage_run = current_runtime * 1.0f / target_runtime * 1.0f;
    if (percentage_run > 1.0f) {
      percentage_run = 1.0f;
    }
    new_value = this->target_value_ - (1.0f - percentage_run) * (this->target_value_ - this->source_value_);
    this->write_internal(new_value);
  }
  if (this->target_value_ == this->current_value_ && this->state_ != 0) {
    this->state_ = 0;
    this->start_millis_ = millis();
    ESP_LOGD(TAG, "Servo reached target");
  }
}

void Servo::write(float value) {
  value = clamp(value, -1.0f, 1.0f);
  this->target_value_ = value;
  this->source_value_ = this->current_value_;
  this->state_ = 1;  // moving
  this->start_millis_ = millis();
  ESP_LOGD(TAG, "Servo new target: %f", value);
}

void Servo::write_internal(float value) {
  value = clamp(value, -1.0f, 1.0f);
  float level;
  if (value < 0.0)
    level = lerp(-value, this->idle_level_, this->min_level_);
  else
    level = lerp(value, this->idle_level_, this->max_level_);

  this->output_->set_level(level);
  this->save_level_(level);
  this->current_value_ = value;
}

}  // namespace servo
}  // namespace esphome
