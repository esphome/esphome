#pragma once

#include "esphome/components/cover/cover.h"
#include "esphome/core/component.h"
#include "../hoermann.h"

namespace esphome::hoermann {

class HoermannCover : public cover::Cover, public Component {
 public:
  void setup() override;
  void set_parent(Hoermann *parent) { this->parent_ = parent; }
  cover::CoverTraits get_traits() override;
  void control(const cover::CoverCall &call) override;

 protected:
  void update_from_state_();
  Hoermann *parent_{nullptr};
  float previous_position_{0.0f};
  cover::CoverOperation previous_operation_{cover::COVER_OPERATION_IDLE};
};

}  // namespace esphome::hoermann
