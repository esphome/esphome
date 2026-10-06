#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../motion_component.h"

namespace esphome::motion {

enum MotionBinarySensorType : uint8_t {
  MOTION_BINARY_SENSOR_FACE_UP = 0,
  MOTION_BINARY_SENSOR_FACE_DOWN,
  MOTION_BINARY_SENSOR_FREE_FALL,
  MOTION_BINARY_SENSOR_MOVING,
};

class MotionBinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  explicit MotionBinarySensor(MotionComponent *parent, MotionBinarySensorType type);

  void setup() override;
  void dump_config() override;

  void set_threshold(float threshold) { this->threshold_ = threshold; }
  void set_duration(uint32_t duration) { this->duration_ = duration; }

 protected:
  void process_motion_data_(const MotionData &data);

  /// True when the device is at rest: total acceleration is close to 1g and (if a
  /// gyroscope is present) the angular rate is low. While not stationary the
  /// face_up / face_down orientation is unreliable, so their updates are suspended.
  static bool is_stationary(const MotionData &data);

  MotionComponent *parent_;
  float threshold_{0.0f};
  uint32_t duration_{0};

  // Tracking states
  uint32_t last_event_time_{0};
  uint32_t free_fall_start_time_{0};

  // For derivative/variance tracking
  float last_accel_[3]{NAN, NAN, NAN};
  float last_gyro_[3]{NAN, NAN, NAN};

  MotionBinarySensorType type_;
  bool free_fall_candidate_{false};
};

}  // namespace esphome::motion
