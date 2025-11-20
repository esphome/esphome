#pragma once

#include "../ld2420.h"
#include "esphome/components/select/select.h"

namespace esphome::ld2420 {

class LD2420Select : public Component, public select::Select, public Parented<LD2420Component> {
 public:
  LD2420Select() = default;

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::ld2420
