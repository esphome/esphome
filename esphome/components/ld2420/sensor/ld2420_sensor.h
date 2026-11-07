#pragma once

#include "../ld2420.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::ld2420 {

class LD2420Sensor final : public LD2420Listener, public Component, public sensor::Sensor {
 public:
  void dump_config() override;
  SUB_SENSOR(distance)
  void on_distance(uint16_t distance) override {
    if (this->distance_sensor_ != nullptr) {
      if (this->distance_sensor_->get_state() != distance) {
        this->distance_sensor_->publish_state(distance);
      }
    }
  }
};

}  // namespace esphome::ld2420
