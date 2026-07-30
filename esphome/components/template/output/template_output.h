#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"

namespace esphome::template_ {

class TemplateBinaryOutput final : public output::BinaryOutput {
 public:
  Trigger<bool> *get_trigger() { return &this->trigger_; }

 protected:
  void write_state(bool state) override { this->trigger_.trigger(state); }

  Trigger<bool> trigger_;
};

class TemplateFloatOutput final : public output::FloatOutput {
 public:
  Trigger<float> *get_trigger() { return &this->trigger_; }

 protected:
  void write_state(float state) override { this->trigger_.trigger(state); }

  Trigger<float> trigger_;
};

}  // namespace esphome::template_
