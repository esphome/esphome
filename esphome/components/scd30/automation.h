#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "scd30.h"

namespace esphome {
namespace scd30 {

template<typename... Ts> class SetForcedRecalibrationValueAction : public Action<Ts...> {
 public:
  explicit SetForcedRecalibrationValueAction(SCD30Component *scd30) : scd30_(scd30) {}

  TEMPLATABLE_VALUE(uint16_t, forced_recalibration_value);

  void play(Ts... x) override {
    this->scd30_->set_forced_recalibration_value(this->forced_recalibration_value_.value(x...));
  }

  SCD30Component *scd30_;
};

}  // namespace scd30
}  // namespace esphome
