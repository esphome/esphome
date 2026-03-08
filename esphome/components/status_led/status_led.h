#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace status_led {

class StatusLED : public Component {
 public:
  explicit StatusLED(GPIOPin *pin);

  void pre_setup();
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override;
#ifdef USE_LOOP_PRIORITY
  float get_loop_priority() const override;
#endif

 protected:
  GPIOPin *pin_;
};

extern StatusLED *global_status_led;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace status_led
}  // namespace esphome
