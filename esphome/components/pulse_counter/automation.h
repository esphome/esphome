#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/pulse_counter/pulse_counter_sensor.h"

namespace esphome {

namespace pulse_counter {

template<typename... Ts> class SetTotalPulsesAction : public Action<Ts...> {
 public:
  SetTotalPulsesAction(PulseCounterSensor *pulse_counter) : pulse_counter_(pulse_counter) {}

  TEMPLATABLE_VALUE(uint32_t, total_pulses)

  void play(Ts... x) override { this->pulse_counter_->set_total_pulses(this->total_pulses_.value(x...)); }

 protected:
  PulseCounterSensor *pulse_counter_;
};

}  // namespace pulse_counter
}  // namespace esphome
