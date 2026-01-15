#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sen5x.h"

namespace esphome {
namespace sen5x {

template<typename... Ts> class StartFanAction : public Action<Ts...>, public Parented<SEN5XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->start_fan_cleaning(); }
};

template<typename... Ts> class ActivateHeaterAction : public Action<Ts...>, public Parented<SEN5XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->activate_heater(); }
};

template<typename... Ts>
class SetAmbientPressureCompensationAction : public Action<Ts...>, public Parented<SEN5XComponent> {
 public:
  void play(const Ts &...x) override {
    auto value = this->value_.value(x...);
    this->parent_->set_ambient_pressure_compensation(value);
  }

 protected:
  TEMPLATABLE_VALUE(uint16_t, value)
};

template<typename... Ts>
class PerformForcedCo2RecalibrationAction : public Action<Ts...>, public Parented<SEN5XComponent> {
 public:
  void play(const Ts &...x) override {
    auto value = this->value_.value(x...);
    this->parent_->perform_forced_co2_recalibration(value);
  }

 protected:
  TEMPLATABLE_VALUE(uint16_t, value)
};

}  // namespace sen5x
}  // namespace esphome
