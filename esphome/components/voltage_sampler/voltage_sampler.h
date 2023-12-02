#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace voltage_sampler {

/// Abstract interface for components to request voltage (usually ADC readings)
class VoltageSampler {
 public:
  /// Get a voltage reading, in V.
  virtual float sample() = 0;
};

}  // namespace voltage_sampler
}  // namespace esphome
