#pragma once

#include "../pipsolar.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/component.h"

namespace esphome {
namespace pipsolar {

class Pipsolar;

class PipsolarOutput : public output::FloatOutput {
 public:
  PipsolarOutput() {}
  void set_parent(Pipsolar *parent) { parent_ = parent; }
  void set_set_command(std::string command) { this->set_command_ = command; };
  void add_possible_value(float v) {}
  void set_possible_values(std::vector<float> possible_values) { possible_values_ = possible_values; }
  void set_value(float value) { this->write_state(value); };

 protected:
  void write_state(float state) override;
  std::string set_command_;
  Pipsolar *parent_;
  std::vector<float> possible_values_;
};

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
