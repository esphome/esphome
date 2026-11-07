#pragma once

#include "esphome/components/button/button.h"
#include "../hoermann_hcp.h"

namespace esphome::hoermann_hcp {

// The door commands the cover has no equivalent for. A refused command is already reported by the hub and
// leaves nothing to correct here, because a button carries no state of its own.
class HoermannHcpButton : public button::Button {
 public:
  explicit HoermannHcpButton(HoermannHcp *parent) : parent_(parent) {}

 protected:
  HoermannHcp *const parent_;
};

class HoermannHcpVentButton final : public HoermannHcpButton {
 public:
  using HoermannHcpButton::HoermannHcpButton;

 protected:
  void press_action() override { this->parent_->vent_door(); }
};

class HoermannHcpHalfOpenButton final : public HoermannHcpButton {
 public:
  using HoermannHcpButton::HoermannHcpButton;

 protected:
  void press_action() override { this->parent_->half_open_door(); }
};

}  // namespace esphome::hoermann_hcp
