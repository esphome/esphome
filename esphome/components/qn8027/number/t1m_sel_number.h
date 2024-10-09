#pragma once

#include "esphome/components/number/number.h"
#include "../qn8027.h"

namespace esphome {
namespace qn8027 {

class T1mSelNumber : public number::Number, public Parented<QN8027Component> {
 public:
  T1mSelNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace qn8027
}  // namespace esphome
