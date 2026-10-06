#pragma once

#include "esphome/components/select/select.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class DistanceResolutionSelect final : public select::Select, public Parented<LD2412Component> {
 public:
  // User provided, not "= default": `new(p) DistanceResolutionSelect()` would zero-fill .bss that is already zero.
  DistanceResolutionSelect() {}

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::ld2412
