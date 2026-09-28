#pragma once
#include "esphome/components/switch/switch.h"
#include "../ld6004.h"
namespace esphome::ld6004 {
class LD6004Switch final : public switch_::Switch {
 public:
  explicit LD6004Switch(LD6004Component *parent) : parent_(parent) {}

 protected:
  void write_state(bool value) override { this->parent_->set_output(value); }
  LD6004Component *parent_;
};
}  // namespace esphome::ld6004
