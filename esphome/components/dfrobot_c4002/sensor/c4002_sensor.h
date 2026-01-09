#pragma once

#include "../dfrobot_c4002.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace dfrobot_c4002 {

class C4002Sensor : public C4002Listener, public Component, sensor::Sensor {
 public:
  void setup() override {
    if (movement_distance_)
      this->movement_distance_->publish_state(0.0f);
    if (existing_distance_)
      this->existing_distance_->publish_state(0.0f);
    if (movement_speed_)
      this->movement_speed_->publish_state(0.0f);
    if (movement_direction_)
      this->movement_direction_->publish_state(0.0f);
    if (target_status_)
      this->target_status_->publish_state(0.0f);
  }
  void set_movement_distance_sensor(sensor::Sensor *sensor) { this->movement_distance_ = sensor; }
  void set_existing_distance_sensor(sensor::Sensor *sensor) { this->existing_distance_ = sensor; }
  void set_movement_speed_sensor(sensor::Sensor *sensor) { this->movement_speed_ = sensor; }
  void set_movement_direction_sensor(sensor::Sensor *sensor) { this->movement_direction_ = sensor; }
  void set_target_status_sensor(sensor::Sensor *sensor) { this->target_status_ = sensor; }

  void on_movement_distance(float distance) override {
    if (this->movement_distance_ != nullptr) {
      if (this->movement_distance_->get_state() != distance) {
        this->movement_distance_->publish_state(distance);
      }
    }
  }

  void on_existing_distance(float distance) override {
    if (this->existing_distance_ != nullptr) {
      if (this->existing_distance_->get_state() != distance) {
        this->existing_distance_->publish_state(distance);
      }
    }
  }

  void on_movement_speed(float speed) override {
    if (this->movement_speed_ != nullptr) {
      if (this->movement_speed_->get_state() != speed) {
        this->movement_speed_->publish_state(speed);
      }
    }
  }

  void on_movement_direction(float direction) override {
    if (this->movement_direction_ != nullptr) {
      if (this->movement_direction_->get_state() != direction) {
        this->movement_direction_->publish_state(direction);
      }
    }
  }

  void on_target_status(uint8_t state) override {
    if (this->target_status_ != nullptr) {
      if (this->target_status_->get_state() != state) {
        this->target_status_->publish_state(state);
      }
    }
  }

 protected:
  sensor::Sensor *movement_distance_{nullptr};
  sensor::Sensor *existing_distance_{nullptr};
  sensor::Sensor *movement_speed_{nullptr};
  sensor::Sensor *movement_direction_{nullptr};
  sensor::Sensor *target_status_{nullptr};
};

}  // namespace dfrobot_c4002
}  // namespace esphome
