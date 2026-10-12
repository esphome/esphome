#pragma once

#include "esphome/core/defines.h"
#ifdef USE_SELECT

#include "ld2410s.h"
#include "esphome/components/select/select.h"

namespace esphome::ld2410s {

class LD2410SSelect final : public select::Select, public Parented<LD2410SComponent> {
 protected:
  void control(size_t index) override {
    this->parent_->set_parameter(Parameter::RESPONSE_SPEED, index == 0 ? RESPONSE_SPEED_NORMAL : RESPONSE_SPEED_FAST);
    this->publish_state(index);
  }
};

}  // namespace esphome::ld2410s

#endif  // USE_SELECT
