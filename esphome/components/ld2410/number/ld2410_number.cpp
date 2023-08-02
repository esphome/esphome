#include "esphome/core/log.h"
#include "ld2410_number.h"

namespace esphome {
namespace ld2410 {

static const char *const TAG = "ld2410.number";

void LD2410Number::setup() {
  uint16_t val;
  if (parent_ == nullptr)
    return;
  if (gate_type_ == LD2410_THRES_MOVE || gate_type_ == LD2410_THRES_STILL) {
    val = parent_->get_threshold(this->gate_num_, this->gate_type_);
  } else {
    val = parent_->get_max_distance_timeout(this->gate_type_);
  }
  this->publish_state(val);
}

void LD2410Number::control(float value) {
  if (parent_ == nullptr)
    return;
  if (gate_type_ == LD2410_THRES_MOVE || gate_type_ == LD2410_THRES_STILL) {
    parent_->set_threshold(this->gate_num_, this->gate_type_, (uint8_t) value);
    ESP_LOGV(TAG, "Setting number to gate %u, type %u: %f", this->gate_num_, this->gate_type_, value);
  } else {
    parent_->set_max_distances_timeout(this->gate_type_, (uint16_t) value);
    ESP_LOGV(TAG, "Setting type %u: %f", this->gate_type_, value);
  }
  this->publish_state(value);
}

void LD2410Number::dump_config() {
  LOG_NUMBER(TAG, "LD2410 Number", this);
  ESP_LOGCONFIG(TAG, "  Number is for gate %u, type %u", this->gate_num_, gate_type_);
}

}  // namespace ld2410
}  // namespace esphome
