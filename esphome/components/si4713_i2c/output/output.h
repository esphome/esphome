#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "../si4713.h"

namespace esphome {
namespace si4713 {

class BinaryOutput : public output::BinaryOutput, public Component, public Parented<Si4713Component> {
 public:
  void set_pin(uint8_t pin) { this->pin_ = pin - 1; }

 protected:
  void write_state(bool state) override {
    this->parent_->set_output_gpio(this->pin_, state);
  }

  uint8_t pin_{0};
};

}  // namespace si4713
}  // namespace esphome
