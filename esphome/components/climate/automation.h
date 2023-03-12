#pragma once

#include "esphome/core/automation.h"
#include "climate.h"

namespace esphome {
namespace climate {

template<typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  explicit ControlAction(Climate *climate) : climate_(climate) {}

  TEMPLATABLE_VALUE(ClimateMode, mode)
  TEMPLATABLE_VALUE(float, target_temperature)
  TEMPLATABLE_VALUE(float, target_temperature_low)
  TEMPLATABLE_VALUE(float, target_temperature_high)
  TEMPLATABLE_VALUE(bool, away)
  TEMPLATABLE_VALUE(ClimateFanMode, fan_mode)
  TEMPLATABLE_VALUE(std::string, custom_fan_mode)
  TEMPLATABLE_VALUE(ClimatePreset, preset)
  TEMPLATABLE_VALUE(std::string, custom_preset)
  TEMPLATABLE_VALUE(ClimateSwingMode, swing_mode)

  void play(Ts... x) override {
    auto call = this->climate_->make_call();
    call.set_mode(this->mode_.optional_value(x...));
    call.set_target_temperature(this->target_temperature_.optional_value(x...));
    call.set_target_temperature_low(this->target_temperature_low_.optional_value(x...));
    call.set_target_temperature_high(this->target_temperature_high_.optional_value(x...));
    if (away_.has_value()) {
      call.set_preset(away_.value(x...) ? CLIMATE_PRESET_AWAY : CLIMATE_PRESET_HOME);
    }
    call.set_fan_mode(this->fan_mode_.optional_value(x...));
    call.set_fan_mode(this->custom_fan_mode_.optional_value(x...));
    call.set_preset(this->preset_.optional_value(x...));
    call.set_preset(this->custom_preset_.optional_value(x...));
    call.set_swing_mode(this->swing_mode_.optional_value(x...));
    call.perform();
  }

 protected:
  Climate *climate_;
};

class ControlTrigger : public Trigger<> {
 public:
  ControlTrigger(Climate *climate) {
    climate->add_on_control_callback([this]() { this->trigger(); });
  }
};

class StateTrigger : public Trigger<> {
 public:
  StateTrigger(Climate *climate) {
    climate->add_on_state_callback([this]() { this->trigger(); });
  }
};

}  // namespace climate
}  // namespace esphome
