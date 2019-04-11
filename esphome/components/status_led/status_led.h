#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"

namespace esphome {
namespace status_led {

class StatusLED : public Component {
 public:
  explicit StatusLED(GPIOPin *pin);

  void pre_setup();
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override;
  float get_loop_priority() const override;

 protected:
  GPIOPin *pin_;
};

extern StatusLED *global_status_led;

}  // namespace status_led
}  // namespace esphome
