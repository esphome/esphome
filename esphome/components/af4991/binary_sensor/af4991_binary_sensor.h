#pragma once

#include "esphome/core/component.h"
#include "esphome/components/af4991/af4991.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace af4991 {

class AF4991BinarySensor : public Component, public binary_sensor::BinarySensor {
 public:
  // Set parent value of the switch
  void set_parent(AF4991 *parent) { this->parent_ = parent; }

  void setup() override;
  void dump_config() override;
  void loop() override;

 protected:
  AF4991 *parent_{nullptr};

  uint8_t switch_pin_{24};  // Built in through the I2C Boards firmware
};

}  // namespace af4991
}  // namespace esphome
