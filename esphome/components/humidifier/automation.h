#pragma once

#include "esphome/core/automation.h"
#include "humidifier.h"

namespace esphome {
namespace humidifier {

template<typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  explicit ControlAction(Humidifier *humidifier) : humidifier_(humidifier) {}

  TEMPLATABLE_VALUE(HumidifierMode, mode)
  TEMPLATABLE_VALUE(float, target_humidity)
  TEMPLATABLE_VALUE(float, target_humidity_low)
  TEMPLATABLE_VALUE(float, target_humidity_high)
  TEMPLATABLE_VALUE(bool, away)
  TEMPLATABLE_VALUE(HumidifierPreset, preset)

  void play(Ts... x) override {
    auto call = this->humidifier_->make_call();
    call.set_mode(this->mode_.optional_value(x...));
    call.set_target_humidity(this->target_humidity_.optional_value(x...));
    call.set_target_humidity_low(this->target_humidity_low_.optional_value(x...));
    call.set_target_humidity_high(this->target_humidity_high_.optional_value(x...));
    if (away_.has_value()) {
      call.set_preset(away_.value(x...) ? HUMIDIFIER_PRESET_AWAY : HUMIDIFIER_PRESET_HOME);
    }
    call.set_preset(this->preset_.optional_value(x...));
    call.perform();
  }

 protected:
  Humidifier *humidifier_;
};

class StateTrigger : public Trigger<> {
 public:
  StateTrigger(Humidifier *humidifier) {
    humidifier->add_on_state_callback([this]() { this->trigger(); });
  }
};

}  // namespace humidifier
}  // namespace esphome
