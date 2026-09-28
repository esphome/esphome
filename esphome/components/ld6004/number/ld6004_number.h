#pragma once
#include "esphome/components/number/number.h"
#include "../ld6004.h"
namespace esphome::ld6004 {
class LD6004Number final : public number::Number {
 public:
  LD6004Number(LD6004Component *parent, uint8_t index) : parent_(parent), index_(index) {}

 protected:
  void control(float value) override { this->parent_->set_number(this->index_, value); }
  LD6004Component *parent_;
  uint8_t index_;
};
}  // namespace esphome::ld6004
