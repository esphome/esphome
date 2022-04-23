#pragma once

#include "esphome/core/component.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/hbridge/hbridge.h"

namespace esphome {
namespace hbridge {

// Using PollingComponent as the updates are more consistent and reduces flickering
class HBridgeLightOutput : public HBridge, public light::LightOutput {
 public:
  light::LightTraits get_traits() override;
  void setup() override;
  void loop() override;

  void write_state(light::LightState *state) override;

 protected:
  const char *get_log_tag() override;

  float light_direction_a_duty_ = 0;
  float light_direction_b_duty_ = 0;
  bool direction_a_update_ = false;
};

}  // namespace hbridge
}  // namespace esphome
