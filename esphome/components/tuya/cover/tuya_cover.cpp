#include "esphome/core/log.h"
#include "tuya_cover.h"

namespace esphome {
namespace tuya {

using namespace esphome::cover;

static const char *TAG = "tuya.cover";

void TuyaCover::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Tuya cover '%s'...", this->name_.c_str());
  
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

  // Subscribe to the position report datapoint.
  uint8_t pos_report_id = 0x03;
  if (this->position_report_id_.has_value()) {
    pos_report_id = *this->position_report_id_;
  }
  this->parent_->register_listener(pos_report_id, [this](TuyaDatapoint datapoint) {
    if (datapoint.value_int == 123) {
      ESP_LOGD(TAG, "Ignoring MCU position report - not calibrated");
    } else {
      ESP_LOGD(TAG, "MCU reported position of: %d%%", 100 - datapoint.value_int);
      this->position = 1.0f - (float)datapoint.value_int / 100;
      this->current_operation = CoverOperation::COVER_OPERATION_IDLE;
      this->publish_state();
    }
  });

  // Datapoint 0x07 _should_ inform us when an external trigger causes
  // the operation mode to change (i.e. opening / closing), but it doesn't
  // report a useful value.  As the operation state can only be idle,
  // opening or closing, we can't set an accurate mode, so don't bother to
  // set it at all - it'll always report itself as idle.

  register_service(&TuyaCover::set_direction, "set_direction", {"inverted"});
}

void TuyaCover::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Cover:");
}

void TuyaCover::set_direction(bool inverted) {
  uint8_t direction_id = 0x05;
  if (this->direction_id_.has_value()) {
    direction_id = *this->direction_id_;
  }
  TuyaDatapoint datapoint{};
  datapoint.id = direction_id;
  datapoint.type = TuyaDatapointType::BOOLEAN;
  datapoint.value_bool = inverted;
  this->parent_->set_datapoint_value(datapoint);

  if (inverted) {
    ESP_LOGD(TAG, "Setting direction: inverted");
  } else {
    ESP_LOGD(TAG, "Setting direction: normal");
  }
}

void TuyaCover::set_optimistic(bool optimistic) {
  this->optimistic_ = optimistic;
}

void TuyaCover::control(const CoverCall &call) {
  uint8_t control_id = 0x01;
  if (this->control_id_.has_value()) {
    control_id = *this->control_id_;
  }
  if (call.get_stop()) {
    TuyaDatapoint datapoint{};
    datapoint.id = control_id;
    datapoint.type = TuyaDatapointType::ENUM;
    datapoint.value_enum = 0x01;
    this->parent_->set_datapoint_value(datapoint);
    ESP_LOGD(TAG, "Stopping");
  }
  if (call.get_position().has_value()) {
    auto pos = *call.get_position();

    if (pos == COVER_OPEN) {
      TuyaDatapoint datapoint{};
      datapoint.id = control_id;
      datapoint.type = TuyaDatapointType::ENUM;
      datapoint.value_enum = 0x00;
      this->parent_->set_datapoint_value(datapoint);
      ESP_LOGD(TAG, "Opening");
    } else if (pos == COVER_CLOSED) {
      TuyaDatapoint datapoint{};
      datapoint.id = control_id;
      datapoint.type = TuyaDatapointType::ENUM;
      datapoint.value_enum = 0x02;
      this->parent_->set_datapoint_value(datapoint);
      ESP_LOGD(TAG, "Closing");
    } else {
      uint8_t pos_control_id = 0x02;
      if (this->position_control_id_.has_value()) {
        pos_control_id = *this->position_control_id_;
      }
      TuyaDatapoint datapoint{};
      datapoint.id = pos_control_id;
      datapoint.type = TuyaDatapointType::INTEGER;
      int requested_pos = (int)(pos * 100);
      datapoint.value_int = 100 - requested_pos;
      this->parent_->set_datapoint_value(datapoint);
      ESP_LOGD(TAG, "Setting position: %d%%", requested_pos);
    }

    if (this->optimistic_) {
      this->position = pos;
    }
  }

  this->publish_state();
}

CoverTraits TuyaCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_position(true);
  traits.set_supports_tilt(false);
  return traits;
}

}  // namespace tuya
}  // namespace esphome
