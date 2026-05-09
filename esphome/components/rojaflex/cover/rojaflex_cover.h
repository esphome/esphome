#pragma once

#include "esphome/components/cover/cover.h"
#include "esphome/core/component.h"

#include "../rojaflex.h"

namespace esphome::rojaflex {

class RojaflexCover : public cover::Cover, public Component, public RojaflexDevice {
 public:
  void setup() override;
  void dump_config() override;

  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void sync_from_parent();

 protected:
  void control(const cover::CoverCall &call) override;
  cover::CoverTraits get_traits() override;

  uint8_t channel_{0};
};

}  // namespace esphome::rojaflex
