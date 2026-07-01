#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome::equitherm {

/// Base class for equitherm climate binary sensor entities
class EquithermBinarySensorBase : public binary_sensor::BinarySensor, public Component {
 public:
  void set_restore_value(bool restore_value) { restore_value_ = restore_value; }

 protected:
  void init_state_(bool value);
  void save_state_(bool value);

  bool restore_value_{false};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::equitherm
