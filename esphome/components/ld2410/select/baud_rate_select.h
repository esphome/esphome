#pragma once

#include "esphome/components/select/select.h"
#include "../ld2410.h"

namespace esphome::ld2410 {

class BaudRateSelect : public select::Select, public Parented<LD2410Component> {
 public:
  BaudRateSelect() = default;

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::ld2410
