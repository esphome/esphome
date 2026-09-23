#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/gree/gree.h"

namespace esphome::gree {

class GreeFeatureSwitch final : public switch_::Switch, public Component, public Parented<GreeClimate> {
 public:
  GreeFeatureSwitch(const char *name, GreeFeature feature) : name_(name), feature_(feature) {}

  void setup() override;
  void dump_config() override;
  void write_state(bool state) override;

 protected:
  const char *name_;
  GreeFeature feature_;
};

}  // namespace esphome::gree
