#pragma once

#include "esphome/components/number/number.h"
#include "../ld2450.h"

namespace esphome::ld2450 {

class PresenceTimeoutNumber : public number::Number, public Parented<LD2450Component> {
 public:
  // User provided, not "= default": `new(p) PresenceTimeoutNumber()` would zero-fill .bss that is already zero.
  PresenceTimeoutNumber() {}

 protected:
  void control(float value) override;
};

}  // namespace esphome::ld2450
