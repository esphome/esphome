#pragma once

#include "esphome/components/number/number.h"
#include "../ld6002b.h"

namespace esphome::ld6002b {

class LD6002BNumber : public number::Number, public Parented<LD6002BComponent> {
 public:
  explicit LD6002BNumber(NumberType type) : type_(type) {}

 protected:
  void control(float value) override;

  NumberType type_;
};

}  // namespace esphome::ld6002b
