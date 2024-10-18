#include "rds_deviation_number.h"

namespace esphome {
namespace qn8027 {

void RDSDeviationNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_rds_deviation(value);
}

}  // namespace qn8027
}  // namespace esphome
