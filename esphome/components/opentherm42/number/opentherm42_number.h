#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/number/number.h"

namespace esphome::opentherm42 {

// A number entity whose value is written to the boiler. Like OpenTherm42Switch, the write is
// fire-and-forget -- publish_state() happens immediately, there's no hardware ack to wait for.
// Always restores its last value from flash on boot, falling back to initial_value_ on first boot or
// a corrupt/missing preference -- see CLAUDE.md's write-only-entity rule: the master must be guaranteed
// to display the value it last successfully wrote to the boiler, not silently revert to a fixed default.
class OpenTherm42Number : public number::Number, public Component {
 public:
  void set_initial_value(float initial_value) { this->initial_value_ = initial_value; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;

  float initial_value_{0};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::opentherm42
