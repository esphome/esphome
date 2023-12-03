#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "pasco2.h"

namespace esphome {
namespace pasco2 {

template<typename... Ts> class PerformForcedCalibrationAction : public Action<Ts...>, public Parented<PASCO2Component> {
 public:
  void play(Ts... x) override {
    if (this->value_.has_value()) {
      this->parent_->perform_forced_calibration(this->value_.value(x...));
    }
  }

 protected:
  TEMPLATABLE_VALUE(uint16_t, value)
};

}  // namespace pasco2
}  // namespace esphome
