#include "zone_type_select.h"

namespace esphome::ld2450 {

void ZoneTypeSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_zone_type(static_cast<ZoneType>(index));
}

}  // namespace esphome::ld2450
