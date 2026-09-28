#pragma once
#include "esphome/components/select/select.h"
#include "../ld6004.h"
namespace esphome::ld6004 {
class LD6004Select final : public select::Select {
 public:
  LD6004Select(LD6004Component *parent, uint8_t index) : parent_(parent), index_(index) {}

 protected:
  void control(size_t value) override { this->parent_->set_select(this->index_, value); }
  LD6004Component *parent_;
  uint8_t index_;
};
}  // namespace esphome::ld6004
