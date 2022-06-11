#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "scd4x.h"

namespace esphome {
namespace scd4x {

template<typename... Ts> class PerformForcedCalibrationAction : public Action<Ts...>, public Parented<SCD4XComponent> {
 public:
  void play(Ts... x) override {
    if (this->value_.has_value()) {
      this->parent_->perform_forced_calibration(this->value_.value(x...));
    }
  }

 protected:
  TEMPLATABLE_VALUE(uint16_t, value)
};

template<typename... Ts> class FactoryResetAction : public Action<Ts...>, public Parented<SCD4XComponent> {
 public:
  void play(Ts... x) override { this->parent_->factory_reset(); }
};

}  // namespace scd4x
}  // namespace esphome
