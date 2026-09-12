#pragma once

#include "esphome/components/select/select.h"
#include "../ld2460.h"

namespace esphome::ld2460 {

class InstallationModeSelect : public select::Select, public Parented<LD2460Component> {
 public:
  InstallationModeSelect() = default;

 protected:
  void control(size_t index) override;
};

}  // namespace esphome::ld2460
