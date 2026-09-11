#pragma once

#include "../ld2410s.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace ld2410s {

class LD2410SStatusReportingFreqNumber : public number::Number, public Parented<LD2410S> {
 public:
  LD2410SStatusReportingFreqNumber() = default;

 protected:
  void control(float status_reporting_freq) override {
#ifdef LD2410S_V2
    this->parent_->set_status_reporting_freq(status_reporting_freq);
#endif
  }
};

}  // namespace ld2410s
}  // namespace esphome
