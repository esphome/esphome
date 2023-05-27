#include "gate_threshold_number.h"

namespace esphome {
namespace ld2410 {

void GateThresholdNumber::control(float value) {
  this->publish_state(value);
  this->parent_->set_gate_threshold(this->gate_);
}

void GateThresholdNumber::set_gate(uint8_t gate) { this->gate_ = gate; }

}  // namespace ld2410
}  // namespace esphome
