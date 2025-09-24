#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/output/float_output.h"

namespace esphome {
namespace template_ {

class TemplateBinaryOutput : public output::BinaryOutput {
 public:
  Trigger<bool> *get_trigger() const { return trigger_; }

 protected:
  void write_state(bool state) override { this->trigger_->trigger(state); }

  Trigger<bool> *trigger_ = new Trigger<bool>();
};

class TemplateFloatOutput : public output::FloatOutput {
 public:
  Trigger<float> *get_trigger() const { return trigger_; }

 protected:
  void write_state(float state) override { this->trigger_->trigger(state); }

  Trigger<float> *trigger_ = new Trigger<float>();
};

}  // namespace template_
}  // namespace esphome
