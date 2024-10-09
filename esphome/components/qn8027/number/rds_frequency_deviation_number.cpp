#include "rds_frequency_deviation_number.h"

namespace esphome {
namespace qn8027 {

void RDSFrequencyDeviationNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_rds_frequency_deviation(value);
}

}  // namespace qn8027
}  // namespace esphome
