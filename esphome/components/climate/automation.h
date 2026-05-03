#pragma once

#include "esphome/core/automation.h"
#include "climate.h"

namespace esphome::climate {

// All configured fields are baked into a single stateless lambda whose
// constants live in flash. The action only stores one function pointer
// plus one parent pointer, regardless of how many fields the user set.
// Trigger args are forwarded to the apply function so user lambdas
// (e.g. `target_temperature: !lambda "return x;"`) keep working.
//
// Trigger args are forwarded as `const std::remove_reference_t<Ts> &...`
// (instead of `const Ts &...`) so codegen can emit the same form in the
// apply lambda's parameter list without producing `const T & &` for
// triggers whose Ts already carries a reference (e.g. `std::string &`).
template<typename... Ts> class ControlAction : public Action<Ts...> {
 public:
  using ApplyFn = void (*)(ClimateCall &, const std::remove_reference_t<Ts> &...);
  ControlAction(Climate *climate, ApplyFn apply) : climate_(climate), apply_(apply) {}

  void play(const Ts &...x) override {
    auto call = this->climate_->make_call();
    this->apply_(call, x...);
    call.perform();
  }

 protected:
  Climate *climate_;
  ApplyFn apply_;
};

class ControlTrigger : public Trigger<ClimateCall &> {
 public:
  ControlTrigger(Climate *climate) {
    climate->add_on_control_callback([this](ClimateCall &x) { this->trigger(x); });
  }
};

class StateTrigger : public Trigger<Climate &> {
 public:
  StateTrigger(Climate *climate) {
    climate->add_on_state_callback([this](Climate &x) { this->trigger(x); });
  }
};

}  // namespace esphome::climate
