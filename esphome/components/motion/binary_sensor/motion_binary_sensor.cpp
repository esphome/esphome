#include "motion_binary_sensor.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::motion {

static const char *const TAG = "motion.binary_sensor";

// Thresholds used to decide the device is at rest for face_up / face_down detection.
// While moving (shaking, being picked up) the orientation reading is dominated by
// linear acceleration and cannot be trusted, so those sensors block (hold) instead.
static constexpr float STILL_ACCEL_TOLERANCE = 0.12f;  // max deviation of |accel| from 1g, in g
static constexpr float STILL_GYRO_THRESHOLD = 15.0f;   // max angular rate magnitude, in °/s
static constexpr float GYRO_THRESHOLD_SCALE = 50.0f;   // arbitrary gyro threshold scale, in °/s per g of acceleration

MotionBinarySensor::MotionBinarySensor(MotionComponent *parent, MotionBinarySensorType type)
    : parent_(parent), type_(type) {}

bool MotionBinarySensor::is_stationary_(const MotionData &data) const {
  float ax = data.acceleration[X_AXIS];
  float ay = data.acceleration[Y_AXIS];
  float az = data.acceleration[Z_AXIS];
  if (std::isnan(ax) || std::isnan(ay) || std::isnan(az))
    return false;

  // Total acceleration must be close to 1g; a larger deviation means the device is
  // being accelerated (shaken / moved) and the gravity direction cannot be trusted.
  float mag = std::sqrt(ax * ax + ay * ay + az * az);
  if (std::fabs(mag - 1.0f) > STILL_ACCEL_TOLERANCE)
    return false;

  // If a gyroscope is present, also require the angular rate to be low.
  float gx = data.angular_rate[X_AXIS];
  float gy = data.angular_rate[Y_AXIS];
  float gz = data.angular_rate[Z_AXIS];
  if (!std::isnan(gx) && !std::isnan(gy) && !std::isnan(gz)) {
    float gmag = std::sqrt(gx * gx + gy * gy + gz * gz);
    if (gmag > STILL_GYRO_THRESHOLD)
      return false;
  }
  return true;
}

void MotionBinarySensor::setup() {
  this->parent_->add_listener([this](MotionData const &data) { this->process_motion_data_(data); });
  this->publish_state(false);  // default to false until the first update
}

static const char *const MOTION_BINARY_SENSOR_TYPE_NAMES[] = {
    "face_up",
    "face_down",
    "free_fall",
    "moving",
};
void MotionBinarySensor::dump_config() {
  LOG_BINARY_SENSOR("", "Motion Binary Sensor", this);
  ESP_LOGCONFIG(TAG, "  Type: %s", MOTION_BINARY_SENSOR_TYPE_NAMES[this->type_]);
  ESP_LOGCONFIG(TAG, "  Threshold: %.3f", this->threshold_);
  ESP_LOGCONFIG(TAG, "  Duration: %" PRIu32 " ms", this->duration_);
}

void MotionBinarySensor::process_motion_data_(const MotionData &data) {
  uint32_t now = millis();

  switch (this->type_) {
    case MOTION_BINARY_SENSOR_FACE_UP:
    case MOTION_BINARY_SENSOR_FACE_DOWN: {
      // Block while the device is moving: hold the last stable state instead of
      // reacting to transient acceleration spikes from shaking or handling.
      if (!this->is_stationary_(data))
        break;

      float ax = data.acceleration[X_AXIS];
      float ay = data.acceleration[Y_AXIS];
      float az = data.acceleration[Z_AXIS];
      float mag = std::sqrt(ax * ax + ay * ay + az * az);
      // is_stationary_() guarantees mag is close to 1g, so this is just a safety net.
      if (mag < 0.1f)
        break;

      // threshold_ is the cosine of the maximum tilt: face_up / face_down are only
      // reported when the device is within that tilt of horizontal. Beyond it, both
      // sensors read false. Normalising by the magnitude makes the tilt limit
      // independent of any residual acceleration.
      float cos_tilt = az / mag;
      if (this->type_ == MOTION_BINARY_SENSOR_FACE_UP) {
        this->publish_state(cos_tilt > this->threshold_);
      } else {
        this->publish_state(cos_tilt < -this->threshold_);
      }
      break;
    }
    case MOTION_BINARY_SENSOR_FREE_FALL: {
      float ax = data.acceleration[X_AXIS];
      float ay = data.acceleration[Y_AXIS];
      float az = data.acceleration[Z_AXIS];
      if (std::isnan(ax) || std::isnan(ay) || std::isnan(az)) {
        // Don't let a gap in valid data count towards the free-fall duration.
        this->free_fall_candidate_ = false;
        return;
      }

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

      // Check acceleration delta. Require all three axes to be valid so a NaN on any
      // axis can't poison last_accel_ and silently stop motion detection.
      bool accel_valid = !std::isnan(ax) && !std::isnan(ay) && !std::isnan(az);
      if (accel_valid) {
        if (!std::isnan(this->last_accel_[0])) {
          float dx = ax - this->last_accel_[0];
          float dy = ay - this->last_accel_[1];
          float dz = az - this->last_accel_[2];
          float accel_diff = std::sqrt(dx * dx + dy * dy + dz * dz);
          if (accel_diff > this->threshold_) {
            moving = true;
          }
        }
        this->last_accel_[0] = ax;
        this->last_accel_[1] = ay;
        this->last_accel_[2] = az;
      }

      // Check angular rate delta. Require all three axes to be valid for the same reason.
      bool gyro_valid = !std::isnan(gx) && !std::isnan(gy) && !std::isnan(gz);
      if (gyro_valid) {
        if (!std::isnan(this->last_gyro_[0])) {
          float dgx = gx - this->last_gyro_[0];
          float dgy = gy - this->last_gyro_[1];
          float dgz = gz - this->last_gyro_[2];
          float gyro_diff = std::sqrt(dgx * dgx + dgy * dgy + dgz * dgz);
          if (gyro_diff > this->threshold_ * GYRO_THRESHOLD_SCALE) {
            moving = true;
          }
        }
        this->last_gyro_[0] = gx;
        this->last_gyro_[1] = gy;
        this->last_gyro_[2] = gz;
      }

      // With no usable data this sample, don't assert "not moving" -- just wait for
      // the next one.
      if (!accel_valid && !gyro_valid)
        break;

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
