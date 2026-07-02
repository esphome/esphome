#pragma once

#include "esphome/components/light/light_output.h"
#include "esphome/core/component.h"
#include "../hoermann.h"

namespace esphome::hoermann {

class HoermannLight : public light::LightOutput, public Component {
 public:
  void setup() override;
  void set_parent(Hoermann *parent) { this->parent_ = parent; }
  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

 protected:
  void update_from_state_();
  Hoermann *parent_{nullptr};
  light::LightState *light_state_{nullptr};
  bool reported_on_{false};
};

}  // namespace esphome::hoermann
