#pragma once

#include "esphome/components/button/button.h"
#include "../dfrobot_sen0609.h"

namespace esphome::dfrobot_sen0609 {

class DFRobotSEN0609Button final : public button::Button, public Parented<DFRobotSEN0609Component> {
 public:
  explicit DFRobotSEN0609Button(ButtonAction action) : action_(action) {}

 protected:
  void press_action() override {
    switch (this->action_) {
      case BUTTON_RESTART:
        this->parent_->restart();
        break;
      case BUTTON_FACTORY_RESET:
        this->parent_->factory_reset();
        break;
      case BUTTON_QUERY_PARAMS:
        this->parent_->query_params();
        break;
    }
  }

  ButtonAction action_;
};

}  // namespace esphome::dfrobot_sen0609
