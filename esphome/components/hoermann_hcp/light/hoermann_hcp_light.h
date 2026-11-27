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
  // Value last published and not yet seen come back, so the write carrying it is that publish, not a request.
  optional<bool> published_state_;
  // Set by the first write_state(), which is always the restored state replayed on boot.
  bool boot_replay_done_{false};
};

}  // namespace esphome::hoermann_hcp
