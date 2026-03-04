#include "zone_coordinate_number.h"

namespace esphome::ld2450 {

ZoneCoordinateNumber::ZoneCoordinateNumber(uint8_t zone) : zone_(zone) {}

void ZoneCoordinateNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_zone_coordinate(this->zone_);
}

}  // namespace esphome::ld2450
