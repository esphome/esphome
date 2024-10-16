#include "rds_deviation_number.h"

namespace esphome {
namespace si4713 {

void RdsDeviationNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_rds_deviation(value);
}

}  // namespace si4713
}  // namespace esphome