#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace opentherm {

class CustomNumber : public Component, public number::Number {
 public:
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }
  void set_initial_value(float initial_value) { this->initial_value_ = initial_value; }
  void setup() override;
  void dump_custom_config(const char *prefix, const char *type);

 protected:
  bool restore_value_{false};
  float initial_value_{NAN};
  ESPPreferenceObject pref_;

  void control(float value) override;
};

}  // namespace opentherm
}  // namespace esphome
