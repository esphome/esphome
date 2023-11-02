#pragma once

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#include "esphome/components/ld2450/target.h"

namespace esphome {
namespace ld2450 {

class Target;

class PresenceZone {
 public:
  PresenceZone(int16_t x_begin, int16_t y_begin, int16_t x_end, int16_t y_end)
      : x_begin_(x_begin), y_begin_(y_begin), x_end_(x_end), y_end_(y_end) {}

  /**
   * Check if any of the targets are within this zone.
   */
  void check_targets(const Target *targets, size_t size);

 private:
  int16_t x_begin_, y_begin_, x_end_, y_end_;

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(presence)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(target_count)
#endif
};

}  // namespace ld2450
}  // namespace esphome
