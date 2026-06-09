#include "../motion_event.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::motion {

static const char *const TAG = "motion.event";

MotionEvent::MotionEvent(MotionComponent *parent) : parent_(parent) {}

void MotionEvent::setup() {
  this->parent_->add_listener([this](MotionData &data) { this->process_motion_data_(data); });
}

void MotionEvent::dump_config() {
  LOG_EVENT("", "Motion Event", this);
  ESP_LOGCONFIG(TAG, "  Threshold: %.3f", this->threshold_);
  ESP_LOGCONFIG(TAG, "  Cooldown: %u ms", this->cooldown_);
}

void MotionEvent::process_motion_data_(const MotionData &data) {
  float ax = data.acceleration[X_AXIS];
  float ay = data.acceleration[Y_AXIS];
  float az = data.acceleration[Z_AXIS];
  if (std::isnan(ax) || std::isnan(ay) || std::isnan(az))
    return;

  uint32_t now = millis();

  if (!std::isnan(this->last_accel_[0])) {
    float dx = ax - this->last_accel_[0];
    float dy = ay - this->last_accel_[1];
    float dz = az - this->last_accel_[2];
    float jerk_mag = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (jerk_mag > this->threshold_) {
      if (now - this->last_trigger_time_ >= this->cooldown_) {
        this->trigger("shake");
        this->last_trigger_time_ = now;
      }
    }
  }

  this->last_accel_[0] = ax;
  this->last_accel_[1] = ay;
  this->last_accel_[2] = az;
}

}  // namespace esphome::motion
