#include "tuner_deviation_number.h"

namespace esphome {
namespace si4713 {

void TunerDeviationNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_tuner_deviation(value);
}

}  // namespace si4713
}  // namespace esphome