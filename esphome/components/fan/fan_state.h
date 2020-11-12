#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "fan.h"
#include "fan_traits.h"

namespace esphome {
namespace fan {

class FanState : public Component, public Fan {
 public:
  FanState() = default;
  /// Construct the fan state with name.
  explicit FanState(const std::string &name);

  void setup() override;
  float get_setup_priority() const override;

protected:
  void control() override;

};

}  // namespace fan
}  // namespace esphome
