#pragma once

#include "esphome/components/max6956/max6956.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace max6956 {

class MAX6956;

class MAX6956LedChannel : public output::FloatOutput, public Component {
 public:
  void set_parent(MAX6956 *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { pin_ = pin; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  void write_state(float state) override;
  void write_state(bool state) override;

  MAX6956 *parent_;
  uint8_t pin_;
};

}  // namespace max6956
}  // namespace esphome
