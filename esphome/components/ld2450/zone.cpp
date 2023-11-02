#include "esphome/components/ld2450/defines.h"
#include "esphome/components/ld2450/zone.h"

namespace esphome {
namespace ld2450 {

void PresenceZone::check_targets(const Target *targets, size_t size) {
  uint8_t target_count = 0;
  for (size_t i = 0; i < size; ++i) {
    int16_t x = targets[i].get_x();
    int16_t y = targets[i].get_y();
    target_count += (x_begin_ < x && x < x_end_ && y_begin_ < y && y < y_end_);
  }

  PUBLISH_BINARY_SENSOR(presence, target_count > 0)
  PUBLISH_SENSOR(target_count, target_count)
}

}  // namespace ld2450
}  // namespace esphome
