#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "../si4713.h"

namespace esphome {
namespace si4713 {

class BinaryOutput : public output::BinaryOutput, public Component, public Parented<Si4713Component> {
 public:
  void dump_config() override;

  void set_pin(uint8_t pin) { this->pin_ = pin - 1; }

 protected:
  void write_state(bool state) override;

  uint8_t pin_{0};
};

}  // namespace si4713
}  // namespace esphome
