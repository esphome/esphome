#include "deviation_number.h"

namespace esphome {
namespace qn8027 {

void DeviationNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_deviation(value);
}

}  // namespace qn8027
}  // namespace esphome
