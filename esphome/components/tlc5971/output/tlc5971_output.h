#pragma once

#include "esphome/core/helpers.h"

#include "esphome/components/output/float_output.h"

#include "../tlc5971.h"

namespace esphome {
namespace tlc5971 {

class TLC5971Channel : public output::FloatOutput, public Parented<TLC5971> {
 public:
  void set_channel(uint8_t channel) { this->channel_ = channel; }

 protected:
  void write_state(float state) override;
  uint8_t channel_;
};

}  // namespace tlc5971
}  // namespace esphome
