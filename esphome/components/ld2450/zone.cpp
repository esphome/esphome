#include "esphome/components/ld2450/zone.h"

#include "esphome/components/ld2450/defines.h"

namespace esphome {
namespace ld2450 {

PresenceZone::PresenceZone(int16_t x_begin, int16_t y_begin, int16_t x_end, int16_t y_end)
    : x_begin_(x_begin), y_begin_(y_begin), x_end_(x_end), y_end_(y_end) {}

void PresenceZone::check_targets(const std::array<Target *, NUM_TARGETS> &targets) {
  uint8_t target_count = 0;
  for (Target *target : targets) {
    target_count += (x_begin_ < target->x_ && target->x_ < x_end_ && y_begin_ < target->y_ && target->y_ < y_end_);
  }

  PUBLISH_BINARY_SENSOR(presence, target_count != 0)
  PUBLISH_SENSOR(target_count, target_count)
}

}  // namespace ld2450
}  // namespace esphome
