#pragma once

#include "../dfrobot_c4001.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace dfrobot_c4001 {
static const char *const TAG = "dfrobot_c4001.select";
class C4001Sensor : public c4001Listener, public Component, sensor::Sensor {
 public:
  void setup() override {
    if (this->distance_sensor_ != nullptr) {
      this->distance_sensor_->publish_state(0.0);
    }
    if (this->speed_sensor_ != nullptr) {
      this->speed_sensor_->publish_state(0.0);
    }
  }

  void set_speed_sensor(sensor::Sensor *sensor) { this->speed_sensor_ = sensor; }
  void set_distance_sensor(sensor::Sensor *sensor) { this->distance_sensor_ = sensor; }
  void on_distance(float distance) override {
    if (this->distance_sensor_ != nullptr) {
      if (this->distance_sensor_->get_state() != distance) {
        this->distance_sensor_->publish_state(distance);
      }
    }
  }

  void on_speed(float speed) override {
    if (this->speed_sensor_ != nullptr) {
      if (this->speed_sensor_->get_state() != speed) {
        this->speed_sensor_->publish_state(speed);
      }
    }
  }

 protected:
  sensor::Sensor *distance_sensor_{nullptr};
  sensor::Sensor *speed_sensor_{nullptr};
};

}  // namespace dfrobot_c4001
}  // namespace esphome
