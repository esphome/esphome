#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace bl0940 {

class CalibrationNumber : public number::Number, public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_restore_value(bool restore_value) { this->restore_value_ = restore_value; }

 protected:
  void control(float value) override;
  bool restore_value_{true};

  ESPPreferenceObject pref_;
};

}  // namespace bl0940
}  // namespace esphome
