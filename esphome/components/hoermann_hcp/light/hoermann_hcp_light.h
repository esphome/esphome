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
  void publish_lamp_state_(bool on);

  HoermannHcp *const parent_;
  light::LightState *light_state_{nullptr};
  // The lamp state last published, never the state last requested. Recording requests here would make an
  // unrelated broadcast look like a disagreement and pull the entity back to a value the lamp has left.
  bool reported_on_{false};
};

}  // namespace esphome::hoermann_hcp
