#pragma once

#include "../ld2415h.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace ld2415h {

class LD2415HSensor : public LD2415HListener, public Component, sensor::Sensor {
 public:
  void dump_config() override;
  void set_speed_sensor(sensor::Sensor *sensor) { this->speed_sensor_ = sensor; }
  void on_speed(uint8_t speed) override {
    if (this->speed_sensor_ != nullptr) {
      if (this->speed_sensor_->get_state() != speed) {
        this->speed_sensor_->publish_state(speed);
      }
    }
  }

 protected:
  sensor::Sensor *speed_sensor_{nullptr};
};

}  // namespace ld2415h
}  // namespace esphome
