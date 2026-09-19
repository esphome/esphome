#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/number/number.h"

#include "../ds3231.h"

namespace esphome::ds3231 {

#ifdef USE_DS3231_AGING_OFFSET
class DS3231AgingOffsetNumber final : public number::Number, public Component, public Parented<DS3231Component> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;
};
#endif

#ifdef USE_DS3231_REFRESH_INTERVAL
/// Sets the hub's poll interval (seconds) at runtime.
class DS3231RefreshIntervalNumber final : public number::Number, public Component, public Parented<DS3231Component> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;
};
#endif

}  // namespace esphome::ds3231
