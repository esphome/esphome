#pragma once

#include <cmath>

#include "esphome/components/cover/cover.h"
#include "esphome/core/component.h"
#include "../hoermann_hcp.h"

namespace esphome::hoermann_hcp {

class HoermannHcpCover : public cover::Cover, public Component {
 public:
  explicit HoermannHcpCover(HoermannHcp *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;
  cover::CoverTraits get_traits() override;
  void control(const cover::CoverCall &call) override;

 protected:
  void update_from_state_();
  HoermannHcp *const parent_;
  // NAN until the first position is observed, so no direction is inferred from a baseline that never existed.
  float previous_position_{NAN};
};

}  // namespace esphome::hoermann_hcp
