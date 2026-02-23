#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"

#include "esphome/components/fujitsu_general/fujitsu_general.h"

namespace esphome {
namespace fujitsu_general {

class FujitsuGeneralButton : public button::Button, public Component, public Parented<FujitsuGeneralClimate> {
 public:
  FujitsuGeneralButton(const char *name, uint8_t command_byte) : name_(name), command_byte_(command_byte) {}

  void dump_config() override;

 protected:
  void press_action() override;

  const char *name_;
  uint8_t command_byte_;
};

}  // namespace fujitsu_general
}  // namespace esphome
