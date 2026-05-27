#pragma once

#include "../dfrobot_c4004.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace dfrobot_c4004 {

class C4004FactoryResetButton : public button::Button, public Parented<C4004Component> {
 protected:
  void press_action() override;
};

class C4004ResetButton : public button::Button, public Parented<C4004Component> {
 protected:
  void press_action() override;
};

class C4004SaveInstallSettingsButton : public button::Button, public Parented<C4004Component> {
 protected:
  void press_action() override;
};

class C4004ApplyBoundaryRangeButton : public button::Button, public Parented<C4004Component> {
 protected:
  void press_action() override;
};

class C4004SetTrajectoryRangeModeButton : public button::Button, public Parented<C4004Component> {
 protected:
  void press_action() override;
};

class C4004ClearAllTagsButton : public button::Button, public Parented<C4004Component> {
 protected:
  void press_action() override;
};

class C4004ClearPeopleCountButton : public button::Button, public Parented<C4004Component> {
 protected:
  void press_action() override;
};

}  // namespace dfrobot_c4004
}  // namespace esphome
