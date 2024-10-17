#pragma once

#include "esphome/components/number/number.h"
#include "../qn8027.h"

namespace esphome {
namespace qn8027 {

class XtalCurrentNumber : public number::Number, public Parented<QN8027Component> {
 public:
  XtalCurrentNumber() = default;

 protected:
  void control(float value) override;
};

}  // namespace qn8027
}  // namespace esphome
