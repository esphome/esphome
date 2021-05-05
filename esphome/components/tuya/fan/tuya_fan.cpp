#include "esphome/core/log.h"
#include "esphome/components/fan/fan_helpers.h"
#include "tuya_fan.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.fan";

void TuyaFan::setup() {
  auto traits =
      fan::FanTraits(this->oscillation_id_.has_value(), this->speed_id_.has_value(), this->direction_id_.has_value(), this->speed_count_);
  this->fan_->set_traits(traits);

  if (this->speed_id_.has_value()) {
    this->parent_->register_listener(*this->speed_id_, [this](TuyaDatapoint datapoint) {
      auto call = this->fan_->make_call();
      if (datapoint.value_enum < this->speed_count_)
        call.set_speed(datapoint.value_enum + 1);
      else
        ESP_LOGCONFIG(TAG, "Speed has invalid value %d", datapoint.value_enum);
      ESP_LOGD(TAG, "MCU reported speed of: %d", datapoint.value_enum);
      call.perform();
    });
  }
  if (this->switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](TuyaDatapoint datapoint) {
      auto call = this->fan_->make_call();
      call.set_state(datapoint.value_bool);
      call.perform();
      ESP_LOGD(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
    });
  }
  if (this->oscillation_id_.has_value()) {
    this->parent_->register_listener(*this->oscillation_id_, [this](TuyaDatapoint datapoint) {
      auto call = this->fan_->make_call();
      call.set_oscillating(datapoint.value_bool);
      call.perform();
      ESP_LOGD(TAG, "MCU reported oscillation is: %s", ONOFF(datapoint.value_bool));
    });
  }
  if (this->direction_id_.has_value()) {
    this->parent_->register_listener(*this->direction_id_, [this](TuyaDatapoint datapoint) {
      auto call = this->fan_->make_call();
      call.set_direction(static_cast<fan::FanDirection>(datapoint.value_bool));
      call.perform();
      ESP_LOGD(TAG, "MCU reported reverse direction is: %s", ONOFF(datapoint.value_bool));
    });
  }

  this->fan_->add_on_state_callback([this]() { this->write_state(); });
}

void TuyaFan::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Fan:");
  ESP_LOGCONFIG(TAG, "  Speed count %d", this->speed_count_);
  if (this->speed_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Speed has datapoint ID %u", *this->speed_id_);
  if (this->switch_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *this->switch_id_);
  if (this->oscillation_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Oscillation has datapoint ID %u", *this->oscillation_id_);
  if (this->direction_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Direction has datapoint ID %u", *this->direction_id_);
}

void TuyaFan::write_state() {
  if (this->switch_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->switch_id_;
    datapoint.type = TuyaDatapointType::BOOLEAN;
    datapoint.value_bool = this->fan_->state;
    this->parent_->set_datapoint_value(datapoint);
    ESP_LOGD(TAG, "Setting switch: %s", ONOFF(this->fan_->state));
  }
  if (this->oscillation_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->oscillation_id_;
    datapoint.type = TuyaDatapointType::BOOLEAN;
    datapoint.value_bool = this->fan_->oscillating;
    this->parent_->set_datapoint_value(datapoint);
    ESP_LOGD(TAG, "Setting oscillating: %s", ONOFF(this->fan_->oscillating));
  }
  if (this->direction_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->direction_id_;
    datapoint.type = TuyaDatapointType::BOOLEAN;
    bool enable = this->fan_->direction == fan::FAN_DIRECTION_REVERSE;
    datapoint.value_bool = enable;
    this->parent_->set_datapoint_value(datapoint);
    ESP_LOGD(TAG, "Setting reverse direction: %s", ONOFF(enable));
  }
  if (this->speed_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->speed_id_;
    datapoint.type = TuyaDatapointType::ENUM;
    datapoint.value_enum = this->fan_->speed - 1;
    ESP_LOGD(TAG, "Setting speed: %d", datapoint.value_enum);
    this->parent_->set_datapoint_value(datapoint);
  }
}

}  // namespace tuya
}  // namespace esphome
