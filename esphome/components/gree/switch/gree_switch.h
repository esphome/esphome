#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/gree/gree.h"

namespace esphome {
namespace gree {

class GreeModeBitSwitch : public switch_::Switch, public Component, public Parented<GreeClimate> {
 public:
  GreeModeBitSwitch(const char *name, uint8_t bit_mask) : name_(name), bit_mask_(bit_mask) {}

  void setup() override;
  void dump_config() override;
  void write_state(bool state) override;

 protected:
  const char *name_;
  uint8_t bit_mask_;
};

}  // namespace gree
}  // namespace esphome
