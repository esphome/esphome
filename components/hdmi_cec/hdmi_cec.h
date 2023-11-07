#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace hdmi_cec {

class HDMICEC : public Component {
public:
  void set_cec_pin(InternalGPIOPin *cec_pin) { cec_pin_ = cec_pin; };

  // Component overrides
  float get_setup_priority() { return esphome::setup_priority::HARDWARE; }
  void setup() override;
  void dump_config() override;
  void loop() override;

protected:
  InternalGPIOPin *cec_pin_;
};

}
}
