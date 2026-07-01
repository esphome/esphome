#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/number/number.h"

namespace esphome::equitherm {

/// Base class for equitherm climate number entities with common restore logic.
///
/// These numbers are runtime tuning interfaces for parameters defined in the climate
/// component's YAML config. The initial value is read from the parent climate in setup().
/// If restore_value is true, user's runtime tuning persists across reboots.
class EquithermNumberBase : public number::Number, public Component {
 public:
  void set_restore_value(bool restore_value) { restore_value_ = restore_value; }

 protected:
  void init_state_(float value);
  void save_state_(float value);

  bool restore_value_{false};
  ESPPreferenceObject pref_;
};

}  // namespace esphome::equitherm
