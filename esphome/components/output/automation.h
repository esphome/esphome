#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace output {

template<typename... Ts> class TurnOffAction : public Action<Ts...> {
 public:
  TurnOffAction(BinaryOutput *output) : output_(output) {}

  void play(const Ts &...x) override { this->output_->turn_off(); }

 protected:
  BinaryOutput *output_;
};

template<typename... Ts> class TurnOnAction : public Action<Ts...> {
 public:
  TurnOnAction(BinaryOutput *output) : output_(output) {}

  void play(const Ts &...x) override { this->output_->turn_on(); }

 protected:
  BinaryOutput *output_;
};

template<typename... Ts> class SetLevelAction : public Action<Ts...> {
 public:
  SetLevelAction(FloatOutput *output) : output_(output) {}

  TEMPLATABLE_VALUE(float, level)

  void play(const Ts &...x) override { this->output_->set_level(this->level_.value(x...)); }

 protected:
  FloatOutput *output_;
};

template<typename... Ts> class SetMinPowerAction : public Action<Ts...> {
 public:
  SetMinPowerAction(FloatOutput *output) : output_(output) {}

  TEMPLATABLE_VALUE(float, min_power)

  void play(const Ts &...x) override { this->output_->set_min_power(this->min_power_.value(x...)); }

 protected:
  FloatOutput *output_;
};

template<typename... Ts> class SetMaxPowerAction : public Action<Ts...> {
 public:
  SetMaxPowerAction(FloatOutput *output) : output_(output) {}

  TEMPLATABLE_VALUE(float, max_power)

  void play(const Ts &...x) override { this->output_->set_max_power(this->max_power_.value(x...)); }

 protected:
  FloatOutput *output_;
};

}  // namespace output
}  // namespace esphome
