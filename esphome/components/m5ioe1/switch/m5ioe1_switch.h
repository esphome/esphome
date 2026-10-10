#pragma once

#include "esphome/components/switch/switch.h"
#include "../m5ioe1.h"

namespace esphome::m5ioe1 {

class AW8737ASwitch : public switch_::Switch, public Component, public Parented<M5IOE1Component> {
 public:
  void setup() override;
  void dump_config() override;

  void set_pin(uint8_t pin) { this->pin_ = pin; }

  void set_pulse_num(uint8_t pulse_num) { this->pulse_num_ = pulse_num; }

 protected:
  void write_state(bool state) override;

  uint8_t pin_;
  uint8_t pulse_num_;
};

}  // namespace esphome::m5ioe1
