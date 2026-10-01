#pragma once

#include "esphome/components/select/select.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class BaudRateSelect final : public select::Select, public Parented<LD2412Component> {
 public:
  // User provided, not "= default": `new(p) BaudRateSelect()` would zero-fill .bss that is already zero.
  BaudRateSelect() {}

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::ld2412
