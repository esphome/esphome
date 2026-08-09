#pragma once

#include <cmath>

#include "esphome/components/cover/cover.h"
#include "esphome/core/component.h"
#include "../hoermann.h"

namespace esphome::hoermann {

class HoermannCover : public cover::Cover, public Component {
 public:
  explicit HoermannCover(Hoermann *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;
  cover::CoverTraits get_traits() override;
  void control(const cover::CoverCall &call) override;

 protected:
  void update_from_state_();
  Hoermann *parent_;
  // NAN until the first position is observed, so no direction is inferred from a baseline that never existed.
  float previous_position_{NAN};
  cover::CoverOperation previous_operation_{cover::COVER_OPERATION_IDLE};
};

}  // namespace esphome::hoermann
