#include "tuner_antcap_number.h"

namespace esphome {
namespace si4713 {

void TunerAntcapNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_tuner_antcap(value);
}

}  // namespace si4713
}  // namespace esphome