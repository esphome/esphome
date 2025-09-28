#pragma once

#include "esphome/components/number/number.h"
#include "esphome/components/opentherm/hub.h"
#include "esphome/components/opentherm/input.h"
#include "esphome/components/opentherm/message_data.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace opentherm {

// Just a simple number, which stores the number
class OpenthermNumber : public number::Number, public Component, public OpenthermInput, public MessageProcessor {
 protected:
  void control(float value) override;
  void setup() override;
  void dump_config() override;

  float initial_value_{NAN};
  bool restore_value_{false};

  ESPPreferenceObject pref_;

 public:
  void prepare_data_out(OpenthermData &data) const override {
    data.type = MessageType::WRITE_DATA;
    message_data::f88::set(data, this->state);
  }

  const char *get_type_name() const override { return "number"; }

  void set_min_value(float min_value) override { this->traits.set_min_value(min_value); }
  void set_max_value(float max_value) override { this->traits.set_max_value(max_value); }
  void set_initial_value(float initial_value) { initial_value_ = initial_value; }
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }
};

}  // namespace opentherm
}  // namespace esphome
