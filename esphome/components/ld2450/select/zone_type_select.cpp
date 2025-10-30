#include "zone_type_select.h"

namespace esphome {
namespace ld2450 {

void ZoneTypeSelect::control(size_t index) {
  this->publish_state(index);
  this->parent_->set_zone_type(this->option_at(index));
}

}  // namespace ld2450
}  // namespace esphome
