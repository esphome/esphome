#include "gate_config_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *const TAG = "LD2420.number";

namespace esphome {
namespace ld2420 {

void LD2420TimeoutNumber::control(float value) {
  const uint16_t timeout = (uint16_t) value;
  this->publish_state(value);
  this->parent_->new_config_.timeout = timeout;
}

void LD2420MinDistanceNumber::control(float value) {
  if (value >= this->parent_->new_config_.max_gate)
    value = this->parent_->max_gate_distance_number_->state;
  this->parent_->new_config_.min_gate = value;
  this->parent_->refresh_gate_config_numbers();
}

void LD2420MaxDistanceNumber::control(float value) {
  if (value <= this->parent_->new_config_.min_gate)
    value = this->parent_->min_gate_distance_number_->state;
  this->parent_->new_config_.max_gate = value;
  this->parent_->refresh_gate_config_numbers();
}

void LD2420GateSelectNumber::control(float value) {
  const uint8_t gate = (uint8_t) value;
  this->publish_state(value);
  this->parent_->gate_move_threshold_number_->publish_state(parent_->new_config_.move_thresh[gate]);
  this->parent_->gate_still_threshold_number_->publish_state(parent_->new_config_.still_thresh[gate]);
}

void LD2420StillThresholdNumber::control(float value) {
  const uint32_t threshold = (uint32_t) value;
  const uint8_t gate = static_cast<uint8_t>(this->parent_->gate_select_number_->state);
  this->publish_state(value);
  this->parent_->new_config_.still_thresh[gate] = threshold;
  this->parent_->configuration_update = true;
}

void LD2420MoveThresholdNumber::control(float value) {
  const uint32_t threshold = (uint32_t) value;
  const uint8_t gate = static_cast<uint8_t>(this->parent_->gate_select_number_->state);
  this->publish_state(value);
  this->parent_->new_config_.move_thresh[gate] = threshold;
  this->parent_->configuration_update = true;
}

}  // namespace ld2420
}  // namespace esphome
