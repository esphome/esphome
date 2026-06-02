#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "motion_component.h"

namespace esphome::motion {

enum MotionBinarySensorType {
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
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_threshold(float threshold) { this->threshold_ = threshold; }
  void set_duration(uint32_t duration) { this->duration_ = duration; }

 protected:
  void process_motion_data_(const MotionData &data);

  MotionComponent *parent_;
  MotionBinarySensorType type_;
  float threshold_{0.0f};
  uint32_t duration_{0};

  // Tracking states
  uint32_t last_event_time_{0};
  bool free_fall_candidate_{false};
  uint32_t free_fall_start_time_{0};

  // For derivative/variance tracking
  float last_accel_[3]{NAN, NAN, NAN};
  float last_gyro_[3]{NAN, NAN, NAN};
};

}  // namespace esphome::motion
