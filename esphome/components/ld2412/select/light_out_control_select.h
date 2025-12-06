#pragma once

#include "esphome/components/select/select.h"
#include "../ld2412.h"

namespace esphome::ld2412 {

class LightOutControlSelect : public select::Select, public Parented<LD2412Component> {
 public:
  LightOutControlSelect() = default;

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::ld2412
