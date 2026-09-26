#pragma once

#include "../ld2410s.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410s {

class LD2410SDelayNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SDelayNumber() = default;

 protected:
  void control(float delay) override {
#ifdef LD2410S_V2
    this->parent_->set_delay(delay);
#endif
  }
};

}  // namespace ld2410s
}  // namespace esphome
