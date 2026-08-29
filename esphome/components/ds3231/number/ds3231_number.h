#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/number/number.h"

#include "../ds3231.h"

#ifdef USE_DS3231_AGING_OFFSET

namespace esphome::ds3231 {

class DS3231AgingOffsetNumber : public number::Number, public Component, public Parented<DS3231Component> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;
};

}  // namespace esphome::ds3231

#endif  // USE_DS3231_AGING_OFFSET
