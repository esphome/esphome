#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "pipsolar_output.h"

namespace esphome {
namespace pipsolar {

template<typename... Ts> class SetOutputAction : public Action<Ts...> {
 public:
  SetOutputAction(PipsolarOutput *output) : output_(output) {}

  TEMPLATABLE_VALUE(float, level)

  void play(Ts... x) override { this->output_->set_value(this->level_.value(x...)); }

 protected:
  PipsolarOutput *output_;
};

}  // namespace pipsolar
}  // namespace esphome
