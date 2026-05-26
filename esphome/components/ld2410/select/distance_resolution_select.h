#pragma once

#include "esphome/components/select/select.h"
#include "../ld2410.h"

namespace esphome::ld2410 {

class DistanceResolutionSelect : public select::Select, public Parented<LD2410Component> {
 public:
  DistanceResolutionSelect() = default;

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::ld2410
