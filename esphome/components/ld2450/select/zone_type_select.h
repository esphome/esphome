#pragma once

#include "esphome/components/select/select.h"
#include "../ld2450.h"

namespace esphome::ld2450 {

class ZoneTypeSelect : public select::Select, public Parented<LD2450Component> {
 public:
  ZoneTypeSelect() = default;

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::ld2450
