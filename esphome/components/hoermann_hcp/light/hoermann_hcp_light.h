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
  // What the entity is showing. While awaiting_lamp_ is set this is a request the lamp has not caught up with
  // yet, so it must not be reconciled away; otherwise it is the lamp state the door last reported.
  bool shown_on_{false};
  bool awaiting_lamp_{false};
  // Cleared once the lamp has been read at least once, until then a write here is the restored state coming
  // back rather than a user request.
  bool synced_{false};
};

}  // namespace esphome::hoermann_hcp
