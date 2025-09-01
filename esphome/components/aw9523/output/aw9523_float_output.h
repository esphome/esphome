#pragma once

#include "esphome/components/aw9523/aw9523.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace aw9523 {

class AW9523Component;

class AW9523FloatOutputChannel : public output::FloatOutput, public Component {
 public:
  void set_parent(AW9523Component *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void set_max_current(float max_current) { this->max_current_ = max_current; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;
  AW9523Component *parent_;
  uint8_t pin_;
  float max_current_;
};

}  // namespace aw9523
}  // namespace esphome
