#include "pga_number.h"

namespace esphome {
namespace kt0803 {

void PgaNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_pga(value);
}

}  // namespace kt0803
}  // namespace esphome
