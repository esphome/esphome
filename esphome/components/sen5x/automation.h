#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sen5x.h"

namespace esphome {
namespace sen5x {

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
class PerformForcedCo2CalibrationAction : public Action<Ts...>, public Parented<SEN5XComponent> {
 public:
  void play(const Ts &...x) override {
    auto value = this->value_.value(x...);
    this->parent_->perform_forced_co2_calibration(value);
  }

 protected:
  TEMPLATABLE_VALUE(uint16_t, value)
};

template<typename... Ts> class StartFanCleaningAction : public Action<Ts...>, public Parented<SEN5XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->start_fan_cleaning(); }
};

template<typename... Ts> class ActivateHeaterAction : public Action<Ts...>, public Parented<SEN5XComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->activate_heater(); }
};

template<typename... Ts>
class SetTemperatureCompensationAction : public Action<Ts...>, public Parented<SEN5XComponent> {
 public:
  void play(const Ts &...x) override {
    auto offset = this->offset_.value(x...);
    auto normalized_offset_slope = this->normalized_offset_slope_.value(x...);
    auto time_constant = this->time_constant_.value(x...);
    auto slot = this->slot_.value(x...);
    this->parent_->set_temperature_compensation(offset, normalized_offset_slope, time_constant, slot);
  }

 protected:
  TEMPLATABLE_VALUE(float, offset)
  TEMPLATABLE_VALUE(float, normalized_offset_slope)
  TEMPLATABLE_VALUE(uint16_t, time_constant)
  TEMPLATABLE_VALUE(uint8_t, slot)
};

}  // namespace sen5x
}  // namespace esphome
