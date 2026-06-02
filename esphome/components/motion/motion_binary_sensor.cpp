#include "motion_binary_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::motion {

static const char *const TAG = "motion.binary_sensor";

MotionBinarySensor::MotionBinarySensor(MotionComponent *parent, MotionBinarySensorType type)
    : parent_(parent), type_(type) {}

void MotionBinarySensor::setup() {
  this->parent_->add_listener([this](MotionData &data) { this->process_motion_data_(data); });
}

void MotionBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Motion Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  Type: %d", static_cast<int>(this->type_));
  ESP_LOGCONFIG(TAG, "  Threshold: %.3f", this->threshold_);
  ESP_LOGCONFIG(TAG, "  Duration: %u ms", this->duration_);
}

void MotionBinarySensor::process_motion_data_(const MotionData &data) {
  uint32_t now = millis();

  switch (this->type_) {
    case MOTION_BINARY_SENSOR_FACE_UP: {
      float az = data.acceleration[Z_AXIS];
      if (!std::isnan(az)) {
        this->publish_state(az > this->threshold_);
      }
      break;
    }
    case MOTION_BINARY_SENSOR_FACE_DOWN: {
      float az = data.acceleration[Z_AXIS];
      if (!std::isnan(az)) {
        this->publish_state(az < -this->threshold_);
      }
      break;
    }
    case MOTION_BINARY_SENSOR_FREE_FALL: {
      float ax = data.acceleration[X_AXIS];
      float ay = data.acceleration[Y_AXIS];
      float az = data.acceleration[Z_AXIS];
      if (std::isnan(ax) || std::isnan(ay) || std::isnan(az))
        return;

      float mag = std::sqrt(ax * ax + ay * ay + az * az);
      bool free_falling = mag < this->threshold_;

      if (free_falling) {
        if (!this->free_fall_candidate_) {
          this->free_fall_candidate_ = true;
          this->free_fall_start_time_ = now;
        } else if (now - this->free_fall_start_time_ >= this->duration_) {
          this->publish_state(true);
        }
      } else {
        this->free_fall_candidate_ = false;
        this->publish_state(false);
      }
      break;
    }
    case MOTION_BINARY_SENSOR_MOVING: {
      float ax = data.acceleration[X_AXIS];
      float ay = data.acceleration[Y_AXIS];
      float az = data.acceleration[Z_AXIS];
      float gx = data.angular_rate[X_AXIS];
      float gy = data.angular_rate[Y_AXIS];
      float gz = data.angular_rate[Z_AXIS];

      bool moving = false;

      // Check acceleration delta
      if (!std::isnan(ax) && !std::isnan(this->last_accel_[0])) {
        float dx = ax - this->last_accel_[0];
        float dy = ay - this->last_accel_[1];
        float dz = az - this->last_accel_[2];
        float accel_diff = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (accel_diff > this->threshold_) {
          moving = true;
        }
      }
      if (!std::isnan(ax)) {
        this->last_accel_[0] = ax;
        this->last_accel_[1] = ay;
        this->last_accel_[2] = az;
      }

      // Check angular rate delta
      if (!std::isnan(gx) && !std::isnan(this->last_gyro_[0])) {
        float dgx = gx - this->last_gyro_[0];
        float dgy = gy - this->last_gyro_[1];
        float dgz = gz - this->last_gyro_[2];
        float gyro_diff = std::sqrt(dgx * dgx + dgy * dgy + dgz * dgz);
        if (gyro_diff > this->threshold_ * 50.0f) {
          moving = true;
        }
      }
      if (!std::isnan(gx)) {
        this->last_gyro_[0] = gx;
        this->last_gyro_[1] = gy;
        this->last_gyro_[2] = gz;
      }

      if (moving) {
        this->publish_state(true);
        this->last_event_time_ = now;
      } else {
        if (this->state && (now - this->last_event_time_ >= this->duration_)) {
          this->publish_state(false);
        }
      }
      break;
    }
  }
}

}  // namespace esphome::motion
