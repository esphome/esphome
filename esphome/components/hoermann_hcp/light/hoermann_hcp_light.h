#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"
#include "../hoermann_hcp.h"

namespace esphome::hoermann_hcp {

class HoermannHcpLight : public light::LightOutput, public Component {
 public:
  explicit HoermannHcpLight(HoermannHcp *parent) : parent_(parent) {}

  void setup() override;
  void setup_state(light::LightState *state) override;
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

 protected:
  void update_from_state_();
  void publish_light_state_();

  HoermannHcp *const parent_;
  light::LightState *light_state_{nullptr};
  bool reported_on_{false};
};

}  // namespace esphome::hoermann_hcp
