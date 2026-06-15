#pragma once

#include "esphome/core/helpers.h"

#include "esphome/components/output/float_output.h"

#include "../tlc5947.h"

namespace esphome::tlc5947 {

class TLC5947Channel : public output::FloatOutput, public Parented<TLC5947> {
 public:
  void set_channel(uint16_t channel) { this->channel_ = channel; }

 protected:
  void write_state(float state) override;
  uint16_t channel_;
};

}  // namespace esphome::tlc5947
