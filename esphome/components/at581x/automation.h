#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include "at581x.h"

namespace esphome {
namespace at581x {

template<typename... Ts> class AT581XResetAction : public Action<Ts...>, public Parented<AT581XComponent> {
 public:
  void play(Ts... x) { this->parent_->reset_hardware_frontend(); }
};

template<typename... Ts> class AT581XSettingsAction : public Action<Ts...>, public Parented<AT581XComponent> {
 public:
  TEMPLATABLE_VALUE(int8_t, hw_frontend_reset)
  TEMPLATABLE_VALUE(int, frequency)
  TEMPLATABLE_VALUE(int, sensing_distance)
  TEMPLATABLE_VALUE(int, poweron_selfcheck_time)
  TEMPLATABLE_VALUE(int, power_consumption)
  TEMPLATABLE_VALUE(int, protect_time)
  TEMPLATABLE_VALUE(int, trigger_base)
  TEMPLATABLE_VALUE(int, trigger_keep)
  TEMPLATABLE_VALUE(int, stage_gain)

  void play(Ts... x) {
    if (this->frequency_.has_value()) {
      int v = this->frequency_.value(x...);
      this->parent_->set_frequency(v);
    }
    if (this->sensing_distance_.has_value()) {
      int v = this->sensing_distance_.value(x...);
      this->parent_->set_sensing_distance(v);
    }
    if (this->poweron_selfcheck_time_.has_value()) {
      int v = this->poweron_selfcheck_time_.value(x...);
      this->parent_->set_poweron_selfcheck_time(v);
    }
    if (this->power_consumption_.has_value()) {
      int v = this->power_consumption_.value(x...);
      this->parent_->set_power_consumption(v);
    }
    if (this->protect_time_.has_value()) {
      int v = this->protect_time_.value(x...);
      this->parent_->set_protect_time(v);
    }
    if (this->trigger_base_.has_value()) {
      int v = this->trigger_base_.value(x...);
      this->parent_->set_trigger_base(v);
    }
    if (this->trigger_keep_.has_value()) {
      int v = this->trigger_keep_.value(x...);
      this->parent_->set_trigger_keep(v);
    }
    if (this->stage_gain_.has_value()) {
      int v = this->stage_gain_.value(x...);
      this->parent_->set_stage_gain(v);
    }

    // This actually perform all the modification on the system
    this->parent_->i2c_write_config();

    if (this->hw_frontend_reset_.has_value() && this->hw_frontend_reset_.value(x...) == true) {
      this->parent_->reset_hardware_frontend();
    }
  }
};
}  // namespace at581x
}  // namespace esphome
