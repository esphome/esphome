#pragma once

#include "esphome/core/component.h"
#include "esphome/components/atm90e32/atm90e32.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace atm90e32 {

class ATM90E32Number : public number::Number, public Parented<ATM90E32Component> {
 public:
  void control(float value) override { this->publish_state(value); }
};

}  // namespace atm90e32
}  // namespace esphome
