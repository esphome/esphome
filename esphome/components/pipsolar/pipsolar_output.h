#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "pipsolar.h"
namespace esphome {
namespace pipsolar {

class Pipsolar;

class PipsolarOutput : public output::FloatOutput {
 public:
  PipsolarOutput() {}
  void set_parent(Pipsolar *parent) { parent_ = parent; }
  void set_set_command(String command) { this->set_command_ = command; };
  void add_possible_value(float v) {}
  void set_possible_values(std::vector<float> possible_values) { possible_values_ = possible_values; }
  void set_value(float value) { this->write_state(value); };

 protected:
  void write_state(float state) override;
  String set_command_;
  Pipsolar *parent_;
  std::vector<float> possible_values_;
};

}  // namespace pipsolar
}  // namespace esphome
