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
  // Set once the entity has taken its state from the lamp. Until then a write here is the restored state
  // coming back on boot rather than a user request.
  bool synced_{false};
};

}  // namespace esphome::hoermann_hcp
