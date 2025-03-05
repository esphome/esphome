#include "zone_type_select.h"

namespace esphome {
namespace ld2450 {

void ZoneTypeSelect::control(const std::string &value) {
  this->publish_state(value);
  this->parent_->set_zone_type(state);
}

}  // namespace ld2450
}  // namespace esphome
