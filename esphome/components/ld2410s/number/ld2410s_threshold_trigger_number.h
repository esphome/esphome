#pragma once

#include "../ld2410s.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410s {

class LD2410SThresholdTriggerNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SThresholdTriggerNumber() = default;

 protected:
  void control(float threshold_trigger) override {
#ifdef LD2410S_V2
    this->parent_->set_threshold_trigger(threshold_trigger);
#endif
  }
};

}  // namespace ld2410s
}  // namespace esphome
