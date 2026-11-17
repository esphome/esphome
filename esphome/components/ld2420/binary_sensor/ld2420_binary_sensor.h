#pragma once

#include "../ld2420.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::ld2420 {

class LD2420BinarySensor final : public LD2420Listener, public Component, public binary_sensor::BinarySensor {
 public:
  void dump_config() override;
  SUB_BINARY_SENSOR(presence)
  void on_presence(bool presence) override {
    if (this->presence_binary_sensor_ != nullptr) {
      if (this->presence_binary_sensor_->state != presence)
        this->presence_binary_sensor_->publish_state(presence);
    }
  }
};

}  // namespace esphome::ld2420
