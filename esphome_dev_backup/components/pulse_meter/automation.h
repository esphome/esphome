#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/pulse_meter/pulse_meter_sensor.h"

namespace esphome {

namespace pulse_meter {

template<typename... Ts> class SetTotalPulsesAction : public Action<Ts...> {
 public:
  SetTotalPulsesAction(PulseMeterSensor *pulse_meter) : pulse_meter_(pulse_meter) {}

  TEMPLATABLE_VALUE(uint32_t, total_pulses)

  void play(Ts... x) override { this->pulse_meter_->set_total_pulses(this->total_pulses_.value(x...)); }

 protected:
  PulseMeterSensor *pulse_meter_;
};

}  // namespace pulse_meter
}  // namespace esphome
