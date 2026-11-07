#pragma once

#include "esphome/components/select/select.h"
#include "../ld2450.h"

namespace esphome::ld2450 {

class ZoneTypeSelect : public select::Select, public Parented<LD2450Component> {
 public:
  // User provided, not "= default": `new(p) ZoneTypeSelect()` would zero-fill .bss that is already zero.
  ZoneTypeSelect() {}

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::ld2450
