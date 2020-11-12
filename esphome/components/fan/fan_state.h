#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "fan.h"
#include "fan_traits.h"

namespace esphome {
namespace fan {

class FanState : public Fan {
 public:
  FanState() = default;
  /// Construct the fan state with name.
  explicit FanState(const std::string &name);

};

}  // namespace fan
}  // namespace esphome
