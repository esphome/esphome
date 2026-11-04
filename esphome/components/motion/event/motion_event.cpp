#include "motion_event.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

namespace esphome::motion {

ESPHOME_LOG_TAG(TAG, "motion.event");

MotionEvent::MotionEvent(MotionComponent *parent) : parent_(parent) {}

void MotionEvent::setup() {
  this->parent_->add_listener([this](MotionData const &data) { this->process_motion_data_(data); });
}

void MotionEvent::dump_config() {
  LOG_EVENT("", "Motion Event", this);
  ESP_LOGCONFIG(TAG,
                "  Threshold: %.3f\n"
                "  Cooldown: %" PRIu32 " ms",
                this->threshold_, this->cooldown_);
}

void MotionEvent::process_motion_data_(const MotionData &data) {
  float ax = data.acceleration[X_AXIS];
  float ay = data.acceleration[Y_AXIS];
  float az = data.acceleration[Z_AXIS];
  if (std::isnan(ax) || std::isnan(ay) || std::isnan(az)) {
    // Reset the baseline so the next valid sample doesn't jerk-compare across the gap.
    this->last_accel_[0] = NAN;
    this->last_accel_[1] = NAN;
    this->last_accel_[2] = NAN;
    return;
  }

  uint32_t now = App.get_loop_component_start_time();

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
