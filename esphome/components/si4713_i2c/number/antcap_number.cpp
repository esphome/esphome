#include "antcap_number.h"

namespace esphome {
namespace si4713 {

void AntcapNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_antcap(value);
}

}  // namespace si4713
}  // namespace esphome