#pragma once

#include "esphome/components/fan/fan.h"
#include "esphome/core/component.h"

namespace esphome::legacy_fan_test {

/// Test fan that uses the DEPRECATED FanTraits setters for preset modes.
/// This validates backward compatibility for external components that haven't migrated.
class LegacyFan : public fan::Fan, public Component {
 public:
  void setup() override {
    auto restore = this->restore_state_();
    if (restore.has_value()) {
      restore->apply(*this);
    }
    this->publish_state();
  }

  fan::FanTraits get_traits() override {
    auto traits = fan::FanTraits(false, true, false, 3);

    // DEPRECATED API: setting preset modes directly on FanTraits.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    traits.set_supported_preset_modes({"Turbo", "Silent", "Eco"});
#pragma GCC diagnostic pop

    return traits;
  }

 protected:
  void control(const fan::FanCall &call) override {
    if (call.get_state().has_value()) {
      this->state = *call.get_state();
    }
    if (call.get_speed().has_value()) {
      this->speed = *call.get_speed();
    }
    this->apply_preset_mode_(call);
    this->publish_state();
  }
};

}  // namespace esphome::legacy_fan_test
