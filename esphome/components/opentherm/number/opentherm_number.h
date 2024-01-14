#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace opentherm {

#define SUB_OPENTHERM_NUMBER(name) \
 protected: \
  opentherm::OpenThermNumber *name##_number_{nullptr}; \
\
 public: \
  void set_##name##_number(opentherm::OpenThermNumber *number) { this->name##_number_ = number; }

class OpenThermNumber : public Component, public number::Number {
 public:
  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }
  void set_initial_value(float initial_value) { this->initial_value_ = initial_value; }
  void setup() override;
  void dump_custom_config(const char *prefix);

 protected:
  bool restore_value_{false};
  float initial_value_{NAN};
  ESPPreferenceObject pref_;

  void control(float value) override;
};

}  // namespace opentherm
}  // namespace esphome
