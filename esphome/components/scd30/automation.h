#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "scd30.h"

namespace esphome {
namespace scd30 {

template<typename... Ts> class ForceRecalibrationWithReference : public Action<Ts...>, public Parented<SCD30Component> {
 public:
  void play(Ts... x) override {
    if (this->value_.has_value()) {
      this->parent_->force_recalibration_with_reference(this->value_.value(x...));
    }
  }

 protected:
  TEMPLATABLE_VALUE(uint16_t, value)
};

}  // namespace scd30
}  // namespace esphome
