#include "esphome/core/log.h"
#include "tuya_fan.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.fan";

void TuyaFan::setup() {
  if (this->speed_id_.has_value()) {
    this->parent_->register_listener(*this->speed_id_, [this](const TuyaDatapoint &datapoint) {
      if (datapoint.type == TuyaDatapointType::ENUM) {
        ESP_LOGV(TAG, "MCU reported speed of: %d", datapoint.value_enum);
        if (datapoint.value_enum >= this->speed_count_) {
          ESP_LOGE(TAG, "Speed has invalid value %d", datapoint.value_enum);
        } else {
          this->speed = datapoint.value_enum + 1;
          this->publish_state();
        }
      } else if (datapoint.type == TuyaDatapointType::INTEGER) {
        ESP_LOGV(TAG, "MCU reported speed of: %d", datapoint.value_int);
        this->speed = datapoint.value_int;
        this->publish_state();
      }
      this->speed_type_ = datapoint.type;
    });
  }
  if (this->switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
      this->state = datapoint.value_bool;
      this->publish_state();
    });
  }
  if (this->oscillation_id_.has_value()) {
    this->parent_->register_listener(*this->oscillation_id_, [this](const TuyaDatapoint &datapoint) {
      // Whether data type is BOOL or ENUM, it will still be a 1 or a 0, so the functions below are valid in both
      // scenarios
      ESP_LOGV(TAG, "MCU reported oscillation is: %s", ONOFF(datapoint.value_bool));
      this->oscillating = datapoint.value_bool;
      this->publish_state();

      this->oscillation_type_ = datapoint.type;
    });
  }
  if (this->direction_id_.has_value()) {
    this->parent_->register_listener(*this->direction_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGD(TAG, "MCU reported reverse direction is: %s", ONOFF(datapoint.value_bool));
      this->direction = datapoint.value_bool ? fan::FanDirection::REVERSE : fan::FanDirection::FORWARD;
      this->publish_state();
    });
  }

  this->parent_->add_on_initialized_callback([this]() {
    auto restored = this->restore_state_();
    if (restored)
      restored->to_call(*this).perform();
  });
}

void TuyaFan::dump_config() {
  LOG_FAN("", "Tuya Fan", this);
  if (this->speed_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Speed has datapoint ID %u", *this->speed_id_);
  }
  if (this->switch_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *this->switch_id_);
  }
  if (this->oscillation_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Oscillation has datapoint ID %u", *this->oscillation_id_);
  }
  if (this->direction_id_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Direction has datapoint ID %u", *this->direction_id_);
  }
}

fan::FanTraits TuyaFan::get_traits() {
  return fan::FanTraits(this->oscillation_id_.has_value(), this->speed_id_.has_value(), this->direction_id_.has_value(),
                        this->speed_count_);
}

void TuyaFan::control(const fan::FanCall &call) {
  if (this->switch_id_.has_value() && call.get_state().has_value()) {
    this->parent_->set_boolean_datapoint_value(*this->switch_id_, *call.get_state());
  }
  if (this->oscillation_id_.has_value() && call.get_oscillating().has_value()) {
    if (this->oscillation_type_ == TuyaDatapointType::ENUM) {
      this->parent_->set_enum_datapoint_value(*this->oscillation_id_, *call.get_oscillating());
    } else if (this->oscillation_type_ == TuyaDatapointType::BOOLEAN) {
      this->parent_->set_boolean_datapoint_value(*this->oscillation_id_, *call.get_oscillating());
    }
  }
  if (this->direction_id_.has_value() && call.get_direction().has_value()) {
    bool enable = *call.get_direction() == fan::FanDirection::REVERSE;
    this->parent_->set_enum_datapoint_value(*this->direction_id_, enable);
  }
  if (this->speed_id_.has_value() && call.get_speed().has_value()) {
    if (this->speed_type_ == TuyaDatapointType::ENUM) {
      this->parent_->set_enum_datapoint_value(*this->speed_id_, *call.get_speed() - 1);
    } else if (this->speed_type_ == TuyaDatapointType::INTEGER) {
      this->parent_->set_integer_datapoint_value(*this->speed_id_, *call.get_speed());
    }
  }
}

}  // namespace tuya
}  // namespace esphome
