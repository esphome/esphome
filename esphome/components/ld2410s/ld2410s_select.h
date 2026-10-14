#pragma once

#include "esphome/core/defines.h"
#if defined(USE_SELECT) && defined(USE_LD2410S_PARAMETERS)

#include "ld2410s.h"
#include "esphome/components/select/select.h"

namespace esphome::ld2410s {

class LD2410SSelect final : public select::Select, public Parented<LD2410SComponent> {
 protected:
  void control(size_t index) override {
    if (this->parent_->set_parameter(Parameter::PARAMETER_RESPONSE_SPEED, progmem_read_byte(&RESPONSE_SPEEDS[index])))
      this->publish_state(index);
  }
};

}  // namespace esphome::ld2410s

#endif  // USE_SELECT && USE_LD2410S_PARAMETERS
