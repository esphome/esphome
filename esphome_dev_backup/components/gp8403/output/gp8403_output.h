#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"

#include "esphome/components/gp8403/gp8403.h"

namespace esphome {
namespace gp8403 {

class GP8403Output : public Component, public output::FloatOutput, public Parented<GP8403> {
 public:
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA - 1; }

  void set_channel(uint8_t channel) { this->channel_ = channel; }

  void write_state(float state) override;

 protected:
  uint8_t channel_;
};

}  // namespace gp8403
}  // namespace esphome
