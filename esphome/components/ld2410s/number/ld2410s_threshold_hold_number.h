#pragma once

#include "../ld2410s.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410s {

class LD2410SThresholdHoldNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SThresholdHoldNumber() = default;

 protected:
  void control(float threshold_hold) override {
#ifdef LD2410S_V2
    this->parent_->set_threshold_hold(threshold_hold);
#endif
  }
};

}  // namespace ld2410s
}  // namespace esphome
