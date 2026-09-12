#pragma once

#include "esphome/components/number/number.h"
#include "../ld2460.h"

namespace esphome::ld2460 {

class InstallationHeightNumber : public number::Number, public Parented<LD2460Component> {
 public:
  InstallationHeightNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace esphome::ld2460
