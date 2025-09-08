#pragma once

#include "esphome/components/output/float_output.h"
#include "esphome/components/opentherm/input.h"
#include "esphome/core/log.h"

namespace esphome {
namespace opentherm {

class OpenthermOutput : public output::FloatOutput, public Component, public OpenthermInput {
 protected:
  bool has_state_ = false;
  const char *id_ = nullptr;

  float min_value_, max_value_;

 public:
  float state;

  void set_id(const char *id) { this->id_ = id; }

  void write_state(float state) override;

  bool has_state() { return this->has_state_; };

  void set_min_value(float min_value) override { this->min_value_ = min_value; }
  void set_max_value(float max_value) override { this->max_value_ = max_value; }
  float get_min_value() { return this->min_value_; }
  float get_max_value() { return this->max_value_; }
};

}  // namespace opentherm
}  // namespace esphome
