#pragma once

#include "esphome/components/output/float_output.h"
#include "../m5ioe1.h"

namespace esphome::m5ioe1 {
class M5IOE1FloatOutput : public output::FloatOutput, public Component, public Parented<M5IOE1Component> {
 public:
  void setup() override;
  void dump_config() override;
  void set_channel(M5IOE1PWMChannel channel) { this->channel_ = channel; }
  void set_pwm_polarity(M5IOE1PWMPolarity polar) { this->polar_ = polar; }

 protected:
  void write_state(float state) override;
  M5IOE1PWMChannel channel_;
  M5IOE1PWMPolarity polar_{M5IOE1PWMPolarity::ACTIVE_HIGH};
};

}  // namespace esphome::m5ioe1
