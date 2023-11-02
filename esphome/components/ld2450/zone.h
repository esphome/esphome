#pragma once

#include "esphome/components/ld2450/target.h"
#include "esphome/core/defines.h"  // NUM_TARGETS

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#include <array>
#include <stdint.h>

namespace esphome {
namespace ld2450 {

class Target;

class PresenceZone {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(presence)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(target_count)
#endif
 public:
  PresenceZone(int16_t x_begin, int16_t y_begin, int16_t x_end, int16_t y_end);

  /**
   * Check if any of the targets are within this zone.
   */
  void check_targets(const std::array<Target *, NUM_TARGETS> &targets);

 private:
  const int16_t x_begin_, y_begin_, x_end_, y_end_;
};

}  // namespace ld2450
}  // namespace esphome
