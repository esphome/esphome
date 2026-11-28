#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

#include "../gree.h"

namespace esphome::gree {

class GreeVerticalSwingSelect final : public select::Select, public Component, public GreeVerticalSwingListener {
 public:
  explicit GreeVerticalSwingSelect(GreeClimate *parent) : parent_(parent) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(size_t index) override;
  void on_vertical_swing_position(uint8_t position) override;

  GreeClimate *parent_;
};

}  // namespace esphome::gree
