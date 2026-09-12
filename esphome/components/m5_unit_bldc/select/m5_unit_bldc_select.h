#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "esphome/components/m5_unit_bldc/m5_unit_bldc.h"

namespace esphome::m5_unit_bldc {

/// A `select` entity for the motor's rotation direction ("Forward"/"Backward").
class M5UnitBldcDirectionSelect : public select::Select, public Parented<M5UnitBldc> {
 protected:
  void control(const std::string &value) override;
};

}  // namespace esphome::m5_unit_bldc
