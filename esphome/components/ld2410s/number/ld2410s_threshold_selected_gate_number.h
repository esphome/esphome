#pragma once

#include "../ld2410s.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410s {

class LD2410SThresholdSelectedGateNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SThresholdSelectedGateNumber() = default;

 protected:
  void control(float threshold_selected_gate) override {
#ifdef LD2410S_V2
    this->parent_->set_threshold_selected_gate(threshold_selected_gate);
#endif
  }
};

}  // namespace ld2410s
}  // namespace esphome
