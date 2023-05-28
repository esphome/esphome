#pragma once

#include "../ld2420.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ld2420 {

class LD2420Sensor : public LD2420Listener, public Component, sensor::Sensor {
 public:
  void dump_config() override;
  void set_distance_sensor(sensor::Sensor *sensor) { this->distance_sensor_ = sensor; }
  void on_distance(uint16_t distance) override {
    if (this->distance_sensor_ != nullptr) {
      if (this->distance_sensor_->get_state() != distance) {
        this->distance_sensor_->publish_state(distance);
      }
    }
  }

 protected:
  sensor::Sensor *distance_sensor_{nullptr};
};

}  // namespace ld2420
}  // namespace esphome
