#pragma once

#include "esphome/core/component.h"
#include "light_traits.h"
#include "light_state.h"

namespace esphome {
namespace light {

class LightState;

/// Interface to write LightStates to hardware.
class LightOutput {
 public:
  /// Return the LightTraits of this LightOutput.
  virtual LightTraits get_traits() = 0;

  virtual void setup_state(LightState *state) {}

  virtual void write_state(LightState *state) = 0;
};

}  // namespace light
}  // namespace esphome
