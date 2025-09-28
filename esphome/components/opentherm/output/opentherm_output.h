#pragma once

#include "esphome/components/opentherm/hub.h"
#include "esphome/components/opentherm/input.h"
#include "esphome/components/opentherm/message_data.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace opentherm {

class OpenthermOutput : public output::FloatOutput, public Component, public OpenthermInput, public MessageProcessor {
 protected:
  bool has_state_ = false;

  float min_value_, max_value_;

 public:
  void prepare_data_out(OpenthermData &data) const override {
    data.type = MessageType::WRITE_DATA;
    message_data::f88::set(data, this->state);
  }

  const char *get_type_name() const override { return "output"; }

  float state;

  void write_state(float state) override;

  bool has_state() { return this->has_state_; };

  void set_min_value(float min_value) override { this->min_value_ = min_value; }
  void set_max_value(float max_value) override { this->max_value_ = max_value; }
  float get_min_value() { return this->min_value_; }
  float get_max_value() { return this->max_value_; }
};

}  // namespace opentherm
}  // namespace esphome
