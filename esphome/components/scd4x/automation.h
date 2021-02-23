#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "scd4x.h"

namespace esphome {
namespace scd4x {

template<typename... Ts> class SetForcedRecalibrationValueAction : public Action<Ts...> {
 public:
  explicit SetForcedRecalibrationValueAction(SCD4XComponent *scd4x) : scd4x_(scd4x) {}

  TEMPLATABLE_VALUE(uint16_t, forced_recalibration_value);

  void play(Ts... x) override {
    if (this->forced_recalibration_value_.has_value()) {
      this->scd4x_->set_forced_recalibration_value(this->forced_recalibration_value_.value(x...));
    }
  }

  SCD4XComponent *scd4x_;
};

}  // namespace scd4x
}  // namespace esphome
