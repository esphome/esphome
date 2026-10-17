#pragma once

#include "esphome/components/button/button.h"
#include "esphome/components/fujitsu_general/fujitsu_general.h"
#include "esphome/core/component.h"

namespace esphome::fujitsu_general {

/// Sends one stateless util command, such as a vane step, when pressed.
class FujitsuGeneralButton final : public button::Button, public Parented<FujitsuGeneralClimate> {
 public:
  explicit FujitsuGeneralButton(uint8_t command_byte) : command_byte_(command_byte) {}

 protected:
  void press_action() override;

  uint8_t command_byte_;
};

}  // namespace esphome::fujitsu_general
