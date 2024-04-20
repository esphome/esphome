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
  TEMPLATABLE_VALUE(HumidifierPreset, preset)
  TEMPLATABLE_VALUE(std::string, custom_preset)

  void play(Ts... x) override {
    auto call = this->humidifier_->make_call();
    call.set_mode(this->mode_.optional_value(x...));
    call.set_target_humidity(this->target_humidity_.optional_value(x...));
    call.set_preset(this->preset_.optional_value(x...));
    call.set_preset(this->custom_preset_.optional_value(x...));
    call.perform();
  }

 protected:
  Humidifier *humidifier_;
};

class ControlTrigger : public Trigger<HumidifierCall &> {
 public:
  ControlTrigger(Humidifier *humidifier) {
    humidifier->add_on_control_callback([this](HumidifierCall &x) { this->trigger(x); });
  }
};

class StateTrigger : public Trigger<Humidifier &> {
 public:
  StateTrigger(Humidifier *humidifier) {
    humidifier->add_on_state_callback([this](Humidifier &x) { this->trigger(x); });
  }
};

}  // namespace humidifier
}  // namespace esphome
