#include "distance_resolution_select.h"

namespace esphome::ld2410 {

void DistanceResolutionSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_distance_resolution(
      static_cast<DistanceResolution>(progmem_read_byte(&DISTANCE_RESOLUTIONS[index])));
}

}  // namespace esphome::ld2410
