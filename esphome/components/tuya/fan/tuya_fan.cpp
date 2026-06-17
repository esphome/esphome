#include "esphome/core/log.h"
#include "tuya_fan.h"

namespace esphome::tuya {

static const char *const TAG = "tuya.fan";

void TuyaFan::setup() {
  auto speed_id = this->speed_id_;
  if (speed_id.has_value()) {
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
  auto switch_id = this->switch_id_;
  if (switch_id.has_value()) {
    this->parent_->register_listener(*switch_id, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
      this->state = datapoint.value_bool;
      this->publish_state();
    });
  }
  auto oscillation_id = this->oscillation_id_;
  if (oscillation_id.has_value()) {
    this->parent_->register_listener(*oscillation_id, [this](const TuyaDatapoint &datapoint) {
      // Whether data type is BOOL or ENUM, it will still be a 1 or a 0, so the functions below are valid in both
      // scenarios
      ESP_LOGV(TAG, "MCU reported oscillation is: %s", ONOFF(datapoint.value_bool));
      this->oscillating = datapoint.value_bool;
      this->publish_state();

      this->oscillation_type_ = datapoint.type;
    });
  }
  auto direction_id = this->direction_id_;
  if (direction_id.has_value()) {
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
  auto speed_dp_id = this->speed_id_;
  if (speed_dp_id.has_value()) {
    ESP_LOGCONFIG(TAG, "  Speed has datapoint ID %u", *speed_dp_id);
  }
  auto switch_dp_id = this->switch_id_;
  if (switch_dp_id.has_value()) {
    ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *switch_dp_id);
  }
  auto oscillation_dp_id = this->oscillation_id_;
  if (oscillation_dp_id.has_value()) {
    ESP_LOGCONFIG(TAG, "  Oscillation has datapoint ID %u", *oscillation_dp_id);
  }
  auto direction_dp_id = this->direction_id_;
  if (direction_dp_id.has_value()) {
    ESP_LOGCONFIG(TAG, "  Direction has datapoint ID %u", *direction_dp_id);
  }
}

fan::FanTraits TuyaFan::get_traits() {
  return fan::FanTraits(this->oscillation_id_.has_value(), this->speed_id_.has_value(), this->direction_id_.has_value(),
                        this->speed_count_);
}

void TuyaFan::control(const fan::FanCall &call) {
  auto switch_id = this->switch_id_;
  if (switch_id.has_value()) {
    auto state = call.get_state();
    if (state.has_value()) {
      this->parent_->set_boolean_datapoint_value(*switch_id, *state);
    }
  }
  auto osc_id = this->oscillation_id_;
  if (osc_id.has_value()) {
    auto oscillating = call.get_oscillating();
    if (oscillating.has_value()) {
      if (this->oscillation_type_ == TuyaDatapointType::ENUM) {
        this->parent_->set_enum_datapoint_value(*osc_id, *oscillating);
      } else if (this->oscillation_type_ == TuyaDatapointType::BOOLEAN) {
        this->parent_->set_boolean_datapoint_value(*osc_id, *oscillating);
      }
    }
  }
  auto dir_id = this->direction_id_;
  if (dir_id.has_value()) {
    auto direction = call.get_direction();
    if (direction.has_value()) {
      bool enable = *direction == fan::FanDirection::REVERSE;
      this->parent_->set_enum_datapoint_value(*dir_id, enable);
    }
  }
  auto spd_id = this->speed_id_;
  if (spd_id.has_value()) {
    auto speed = call.get_speed();
    if (speed.has_value()) {
      if (this->speed_type_ == TuyaDatapointType::ENUM) {
        this->parent_->set_enum_datapoint_value(*spd_id, *speed - 1);
      } else if (this->speed_type_ == TuyaDatapointType::INTEGER) {
        this->parent_->set_integer_datapoint_value(*spd_id, *speed);
      }
    }
  }
}

}  // namespace esphome::tuya
