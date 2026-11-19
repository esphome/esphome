#include "gate_config_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

ESPHOME_LOG_TAG(TAG, "ld2420.number");

namespace esphome::ld2420 {

void LD2420MinDistanceNumber::control(float min_gate) {
  if ((uint16_t) min_gate > this->parent_->new_config.max_gate) {
    min_gate = this->parent_->get_min_gate_distance_value();
  } else {
    this->parent_->new_config.min_gate = (uint16_t) min_gate;
  }
  this->publish_state(min_gate);
}

void LD2420MaxDistanceNumber::control(float max_gate) {
  if ((uint16_t) max_gate < this->parent_->new_config.min_gate) {
    max_gate = this->parent_->get_max_gate_distance_value();
  } else {
    this->parent_->new_config.max_gate = (uint16_t) max_gate;
  }
  this->publish_state(max_gate);
}

void LD2420GateSelectNumber::control(float gate_select) {
  const uint8_t gate = (uint8_t) gate_select;
  this->publish_state(gate_select);
  this->parent_->publish_gate_move_threshold(gate);
  this->parent_->publish_gate_still_threshold(gate);
}

}  // namespace esphome::ld2420
