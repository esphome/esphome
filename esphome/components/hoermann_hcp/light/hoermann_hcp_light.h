#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"
#include "../hoermann_hcp.h"

namespace esphome::hoermann_hcp {

class HoermannHcpLight : public light::LightOutput, public Component {
 public:
  void setup() override;
  void set_parent(HoermannHcp *parent) { this->parent_ = parent; }
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

 protected:
  void update_from_state_();
  HoermannHcp *parent_{nullptr};
  light::LightState *light_state_{nullptr};
  bool reported_on_{false};
};

}  // namespace esphome::hoermann_hcp
