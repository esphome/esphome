#pragma once

#include "esphome/components/select/select.h"
#include "../airton.h"

namespace esphome::airton {

class VerticalDirectionSelect : public select::Select, public Parented<AirtonClimate> {
 protected:
  // The options are in the order of the enum, so the index is the direction; the parent publishes the state
  void control(size_t index) override {
    if (this->parent_->set_vertical_direction_state(static_cast<VerticalDirection>(index)))
      this->parent_->send_settings();
  }
};

}  // namespace esphome::airton
