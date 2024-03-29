#include "servo.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <cinttypes>

namespace esphome {
namespace servo {

static const char *const TAG = "servo";

uint32_t global_servo_id = 1911044085ULL;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void Servo::dump_config() {
  ESP_LOGCONFIG(TAG, "Servo:");
  ESP_LOGCONFIG(TAG, "  Idle Level: %.1f%%", this->idle_level_ * 100.0f);
  ESP_LOGCONFIG(TAG, "  Min Level: %.1f%%", this->min_level_ * 100.0f);
  ESP_LOGCONFIG(TAG, "  Max Level: %.1f%%", this->max_level_ * 100.0f);
  ESP_LOGCONFIG(TAG, "  auto detach time: %" PRIu32 " ms", this->auto_detach_time_);
  ESP_LOGCONFIG(TAG, "  run duration: %" PRIu32 " ms", this->transition_length_);
}

void Servo::setup() {
  float v;
  if (this->restore_) {
    this->rtc_ = global_preferences->make_preference<float>(global_servo_id);
    global_servo_id++;
    if (this->rtc_.load(&v)) {
      this->target_value_ = v;
      this->internal_write(v);
      this->state_ = STATE_ATTACHED;
      this->start_millis_ = millis();
      return;
    }
  }
  this->detach();
}

void Servo::loop() {
  // check if auto_detach_time_ is set and servo reached target
  if (this->auto_detach_time_ && this->state_ == STATE_TARGET_REACHED) {
    if (millis() - this->start_millis_ > this->auto_detach_time_) {
      this->detach();
      this->start_millis_ = 0;
      ESP_LOGD(TAG, "Servo detached on auto_detach_time");
    }
  }
  if (this->target_value_ != this->current_value_ && this->state_ == STATE_ATTACHED) {
    if (this->transition_length_) {
      float new_value;
      float travel_diff = this->target_value_ - this->source_value_;
      uint32_t target_runtime = abs((int) ((travel_diff) * this->transition_length_ * 1.0f / 2.0f));
      uint32_t current_runtime = millis() - this->start_millis_;
      float percentage_run = current_runtime * 1.0f / target_runtime * 1.0f;
      if (percentage_run > 1.0f) {
        percentage_run = 1.0f;
      }
      new_value = this->target_value_ - (1.0f - percentage_run) * (this->target_value_ - this->source_value_);
      this->internal_write(new_value);
    } else {
      this->internal_write(this->target_value_);
    }
  }
  if (this->target_value_ == this->current_value_ && this->state_ == STATE_ATTACHED) {
    this->state_ = STATE_TARGET_REACHED;
    this->start_millis_ = millis();  // set current stamp for potential auto_detach_time_ check
    ESP_LOGD(TAG, "Servo reached target");
  }
}

void Servo::write(float value) {
  value = clamp(value, -1.0f, 1.0f);
  if ((this->state_ == STATE_DETACHED) && (this->target_value_ == value)) {
    this->internal_write(value);
  } else {
    this->save_level_(value);
  }
  this->target_value_ = value;
  this->source_value_ = this->current_value_;
  this->state_ = STATE_ATTACHED;
  this->start_millis_ = millis();
  ESP_LOGD(TAG, "Servo new target: %f", value);
}

void Servo::internal_write(float value) {
  value = clamp(value, -1.0f, 1.0f);
  float level;
  if (value < 0.0) {
    level = lerp(-value, this->idle_level_, this->min_level_);
  } else {
    level = lerp(value, this->idle_level_, this->max_level_);
  }
  this->output_->set_level(level);
  this->current_value_ = value;
}

void Servo::detach() {
  this->state_ = STATE_DETACHED;
  this->output_->set_level(0.0f);
}

void Servo::save_level_(float v) {
  if (this->restore_)
    this->rtc_.save(&v);
}

}  // namespace servo
}  // namespace esphome
