#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"

#include "../mpr121.h"

namespace esphome {
namespace mpr121 {

class MPR121BinarySensor : public binary_sensor::BinarySensor, public MPR121Channel, public Parented<MPR121Component> {
 public:
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_touch_threshold(uint8_t touch_threshold) { this->touch_threshold_ = touch_threshold; };
  void set_release_threshold(uint8_t release_threshold) { this->release_threshold_ = release_threshold; };

  void setup() override;
  void process(uint16_t data) override;

 protected:
  uint8_t channel_{0};
  optional<uint8_t> touch_threshold_{};
  optional<uint8_t> release_threshold_{};
};

}  // namespace mpr121
}  // namespace esphome
