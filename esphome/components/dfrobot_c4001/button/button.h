#pragma once

#include "esphome/components/button/button.h"
#include "../dfrobot_c4001.h"

namespace esphome {
namespace dfrobot_c4001 {
class ConfigSaveButton : public button::Button, public Parented<DFRobotC4001Hub> {
 protected:
  void press_action() override;
};
class FactoryResetButton : public button::Button, public Parented<DFRobotC4001Hub> {
 protected:
  void press_action() override;
};
class RestartButton : public button::Button, public Parented<DFRobotC4001Hub> {
 protected:
  void press_action() override;
};

}  // namespace dfrobot_c4001
}  // namespace esphome
