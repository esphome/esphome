#include "gate_threshold_number.h"

namespace esphome {
namespace ld2412 {

GateThresholdNumber::GateThresholdNumber(uint8_t gate) : gate_(gate) {}

void GateThresholdNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_gate_threshold();
}

}  // namespace ld2412
}  // namespace esphome
