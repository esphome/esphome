#include "gate_config_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *const TAG = "LD2420.number";

namespace esphome {
namespace ld2420 {

void LD2420TimeoutNumber::control(float timeout) {
  this->publish_state(timeout);
  this->parent_->new_config.timeout = timeout;
}

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

void LD2420MoveSensFactorNumber::control(float move_factor) {
  this->publish_state(move_factor);
  this->parent_->gate_move_sensitivity_factor = move_factor;
}

void LD2420StillSensFactorNumber::control(float still_factor) {
  this->publish_state(still_factor);
  this->parent_->gate_still_sensitivity_factor = still_factor;
}

LD2420MoveThresholdNumbers::LD2420MoveThresholdNumbers(uint8_t gate) : gate_(gate) {}

void LD2420MoveThresholdNumbers::control(float move_threshold) {
  this->publish_state(move_threshold);
  if (!this->parent_->is_gate_select()) {
    this->parent_->new_config.move_thresh[this->gate_] = move_threshold;
  } else {
    this->parent_->new_config.move_thresh[this->parent_->get_gate_select_value()] = move_threshold;
  }
}

LD2420StillThresholdNumbers::LD2420StillThresholdNumbers(uint8_t gate) : gate_(gate) {}

void LD2420StillThresholdNumbers::control(float still_threshold) {
  this->publish_state(still_threshold);
  if (!this->parent_->is_gate_select()) {
    this->parent_->new_config.still_thresh[this->gate_] = still_threshold;
  } else {
    this->parent_->new_config.still_thresh[this->parent_->get_gate_select_value()] = still_threshold;
  }
}

}  // namespace ld2420
}  // namespace esphome
