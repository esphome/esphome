#pragma once
#include "esphome/components/button/button.h"
#include "../ld6004.h"
namespace esphome::ld6004 {
class LD6004Button final : public button::Button {
 public:
  LD6004Button(LD6004Component *parent, uint8_t index) : parent_(parent), index_(index) {}

 protected:
  void press_action() override {
    if (this->index_ == 1) {
      this->parent_->refresh();
    } else {
      const uint8_t commands[]{0,
                               0,
                               CMD_GENERATE_INTERFERENCE,
                               CMD_CLEAR_INTERFERENCE,
                               CMD_RESET_DETECTION,
                               CMD_CLEAR_DWELL,
                               CMD_RESET_UNOCCUPIED};
      this->parent_->control(commands[this->index_], 0, false);
    }
  }
  LD6004Component *parent_;
  uint8_t index_;
};
}  // namespace esphome::ld6004
