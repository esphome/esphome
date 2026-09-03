#pragma once

#include "../ld2410s.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410s {

class LD2410SThresholdSnrNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SThresholdSnrNumber() = default;

 protected:
  void control(float threshold_snr) override {
#ifdef LD2410S_V2
    this->parent_->set_threshold_snr(threshold_snr);
#endif
  }
};

}  // namespace ld2410s
}  // namespace esphome
