#include "esphome/core/log.h"
#include "tuya_fan.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.fan";

void TuyaFan::setup() {
  if (auto speed_id = this->speed_id_; speed_id.has_value()) {
    this->parent_->register_listener(*speed_id, [this](const TuyaDatapoint &datapoint) {
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
  if (auto switch_id = this->switch_id_; switch_id.has_value()) {
    this->parent_->register_listener(*switch_id, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
      this->state = datapoint.value_bool;
      this->publish_state();
    });
  }
  if (auto oscillation_id = this->oscillation_id_; oscillation_id.has_value()) {
    this->parent_->register_listener(*oscillation_id, [this](const TuyaDatapoint &datapoint) {
      // Whether data type is BOOL or ENUM, it will still be a 1 or a 0, so the functions below are valid in both
      // scenarios
      ESP_LOGV(TAG, "MCU reported oscillation is: %s", ONOFF(datapoint.value_bool));
      this->oscillating = datapoint.value_bool;
      this->publish_state();

      this->oscillation_type_ = datapoint.type;
    });
  }
  if (auto direction_id = this->direction_id_; direction_id.has_value()) {
    this->parent_->register_listener(*direction_id, [this](const TuyaDatapoint &datapoint) {
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
  if (auto id = this->speed_id_; id.has_value()) {
    ESP_LOGCONFIG(TAG, "  Speed has datapoint ID %u", *id);
  }
  if (auto id = this->switch_id_; id.has_value()) {
    ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *id);
  }
  if (auto id = this->oscillation_id_; id.has_value()) {
    ESP_LOGCONFIG(TAG, "  Oscillation has datapoint ID %u", *id);
  }
  if (auto id = this->direction_id_; id.has_value()) {
    ESP_LOGCONFIG(TAG, "  Direction has datapoint ID %u", *id);
  }
}

fan::FanTraits TuyaFan::get_traits() {
  return fan::FanTraits(this->oscillation_id_.has_value(), this->speed_id_.has_value(), this->direction_id_.has_value(),
                        this->speed_count_);
}

void TuyaFan::control(const fan::FanCall &call) {
  if (auto switch_id = this->switch_id_; switch_id.has_value()) {
    if (auto state = call.get_state(); state.has_value()) {
      this->parent_->set_boolean_datapoint_value(*switch_id, *state);
    }
  }
  if (auto osc_id = this->oscillation_id_; osc_id.has_value()) {
    if (auto oscillating = call.get_oscillating(); oscillating.has_value()) {
      if (this->oscillation_type_ == TuyaDatapointType::ENUM) {
        this->parent_->set_enum_datapoint_value(*osc_id, *oscillating);
      } else if (this->oscillation_type_ == TuyaDatapointType::BOOLEAN) {
        this->parent_->set_boolean_datapoint_value(*osc_id, *oscillating);
      }
    }
  }
  if (auto dir_id = this->direction_id_; dir_id.has_value()) {
    if (auto direction = call.get_direction(); direction.has_value()) {
      bool enable = *direction == fan::FanDirection::REVERSE;
      this->parent_->set_enum_datapoint_value(*dir_id, enable);
    }
  }
  if (auto spd_id = this->speed_id_; spd_id.has_value()) {
    if (auto speed = call.get_speed(); speed.has_value()) {
      if (this->speed_type_ == TuyaDatapointType::ENUM) {
        this->parent_->set_enum_datapoint_value(*spd_id, *speed - 1);
      } else if (this->speed_type_ == TuyaDatapointType::INTEGER) {
        this->parent_->set_integer_datapoint_value(*spd_id, *speed);
      }
    }
  }
}

}  // namespace tuya
}  // namespace esphome
