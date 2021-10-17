#include "esphome/core/log.h"
#include "tuya_cover.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.cover";

void TuyaCover::setup() {
  this->value_range_ = this->max_value_ - this->min_value_;
  if (this->position_id_.has_value()) {
    this->parent_->register_listener(*this->position_id_, [this](const TuyaDatapoint &datapoint) {
      auto pos = float(datapoint.value_uint - this->min_value_) / this->value_range_;
      if (this->invert_position_)
        pos = 1.0f - pos;
      this->position = pos;
      this->publish_state();
    });
  }
}

void TuyaCover::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    auto pos = this->position;
    if (this->invert_position_)
      pos = 1.0f - pos;
    auto position_int = static_cast<uint32_t>(pos * this->value_range_);
    position_int = position_int + this->min_value_;

    parent_->set_integer_datapoint_value(*this->position_id_, position_int);
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (this->invert_position_)
      pos = 1.0f - pos;
    auto position_int = static_cast<uint32_t>(pos * this->value_range_);
    position_int = position_int + this->min_value_;

    parent_->set_integer_datapoint_value(*this->position_id_, position_int);
  }

  this->publish_state();
}

void TuyaCover::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Cover:");
  if (this->position_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Position has datapoint ID %u", *this->position_id_);
}

cover::CoverTraits TuyaCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_position(true);
  return traits;
}

}  // namespace tuya
}  // namespace esphome
