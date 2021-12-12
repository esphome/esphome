#include "esphome/core/log.h"
#include "tuya_cover.h"

namespace esphome {
namespace tuya {

const uint8_t COMMAND_OPEN = 0x00;
const uint8_t COMMAND_CLOSE = 0x02;
const uint8_t COMMAND_STOP = 0x01;

using namespace esphome::cover;

static const char *const TAG = "tuya.cover";

void TuyaCover::setup() {
  this->value_range_ = this->max_value_ - this->min_value_;

  this->parent_->add_on_initialized_callback([this]() {
    // Set the direction (if configured/supported).
    this->set_direction_(this->invert_position_);

    // Handle configured restore mode.
    switch (this->restore_mode_) {
      case COVER_NO_RESTORE:
        break;
      case COVER_RESTORE: {
        auto restore = this->restore_state_();
        if (restore.has_value())
          restore->apply(this);
        break;
      }
      case COVER_RESTORE_AND_CALL: {
        auto restore = this->restore_state_();
        if (restore.has_value()) {
          restore->to_call(this).perform();
        }
        break;
      }
    }
  });

  uint8_t report_id = *this->position_id_;
  if (this->position_report_id_.has_value()) {
    // A position report datapoint is configured; listen to that instead.
    report_id = *this->position_report_id_;
  }

  this->parent_->register_listener(report_id, [this](const TuyaDatapoint &datapoint) {
    if (datapoint.value_int == 123) {
      ESP_LOGD(TAG, "Ignoring MCU position report - not calibrated");
      return;
    }
    auto pos = float(datapoint.value_uint - this->min_value_) / this->value_range_;
    this->position = 1.0f - pos;
    this->publish_state();
  });
}

void TuyaCover::control(const cover::CoverCall &call) {
  if (call.get_stop()) {
    if (this->control_id_.has_value()) {
      this->parent_->force_set_enum_datapoint_value(*this->control_id_, COMMAND_STOP);
    } else {
      auto pos = this->position;
      pos = 1.0f - pos;
      auto position_int = static_cast<uint32_t>(pos * this->value_range_);
      position_int = position_int + this->min_value_;

      parent_->set_integer_datapoint_value(*this->position_id_, position_int);
    }
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    if (this->control_id_.has_value() && (pos == COVER_OPEN || pos == COVER_CLOSED)) {
      if (pos == COVER_OPEN) {
        this->parent_->force_set_enum_datapoint_value(*this->control_id_, COMMAND_OPEN);
      } else {
        this->parent_->force_set_enum_datapoint_value(*this->control_id_, COMMAND_CLOSE);
      }
    } else {
      pos = 1.0f - pos;
      auto position_int = static_cast<uint32_t>(pos * this->value_range_);
      position_int = position_int + this->min_value_;

      parent_->set_integer_datapoint_value(*this->position_id_, position_int);
    }
  }

  this->publish_state();
}

void TuyaCover::set_direction_(bool inverted) {
  if (!this->direction_id_.has_value()) {
    return;
  }

  if (inverted) {
    ESP_LOGD(TAG, "Setting direction: inverted");
  } else {
    ESP_LOGD(TAG, "Setting direction: normal");
  }

  this->parent_->set_boolean_datapoint_value(*this->direction_id_, inverted);
}

void TuyaCover::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Cover:");
  if (this->invert_position_) {
    if (this->direction_id_.has_value()) {
      ESP_LOGCONFIG(TAG, "   Inverted");
    } else {
      ESP_LOGCONFIG(TAG, "   Configured as Inverted, but direction_datapoint isn't configured");
    }
  }
  if (this->control_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Control has datapoint ID %u", *this->control_id_);
  if (this->direction_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Direction has datapoint ID %u", *this->direction_id_);
  if (this->position_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Position has datapoint ID %u", *this->position_id_);
  if (this->position_report_id_.has_value())
    ESP_LOGCONFIG(TAG, "   Position Report has datapoint ID %u", *this->position_report_id_);
}

cover::CoverTraits TuyaCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_supports_position(true);
  return traits;
}

}  // namespace tuya
}  // namespace esphome
