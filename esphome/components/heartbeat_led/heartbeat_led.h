#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"

namespace esphome {
namespace heartbeat_led {

class HeartbeatLED : public Component {
 public:
  explicit HeartbeatLED(GPIOPin *pin);

  void pre_setup();
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override;
  float get_loop_priority() const override;

 protected:
  GPIOPin *pin_;
};

extern HeartbeatLED *global_heartbeat_led;

}  // namespace heartbeat_led
}  // namespace esphome
