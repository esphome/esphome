#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace custom {

class CustomBinaryOutputConstructor {
 public:
  CustomBinaryOutputConstructor(const std::function<std::vector<output::BinaryOutput *>()> &init) {
    this->outputs_ = init();
  }

  output::BinaryOutput *get_output(int i) { return this->outputs_[i]; }

 protected:
  std::vector<output::BinaryOutput *> outputs_;
};

class CustomFloatOutputConstructor {
 public:
  CustomFloatOutputConstructor(const std::function<std::vector<output::FloatOutput *>()> &init) {
    this->outputs_ = init();
  }

  output::FloatOutput *get_output(int i) { return this->outputs_[i]; }

 protected:
  std::vector<output::FloatOutput *> outputs_;
};

}  // namespace custom
}  // namespace esphome
