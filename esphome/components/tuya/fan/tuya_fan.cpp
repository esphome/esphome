#include "esphome/core/log.h"
#include "tuya_fan.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.fan";

void TuyaFan::setup() {
  auto traits = fan::FanTraits(this->oscillation_id_.has_value(), this->speed_id_.has_value(), false);
  this->fan_->set_traits(traits);

  if (this->speed_id_.has_value()) {
    this->parent_->register_listener(*this->speed_id_, [this](TuyaDatapoint datapoint) {
      auto call = this->fan_->make_call();
      if (datapoint.value_enum == 0x0)
        call.set_speed(fan::FAN_SPEED_LOW);
      else if (datapoint.value_enum == 0x1)
        call.set_speed(fan::FAN_SPEED_MEDIUM);
      else if (datapoint.value_enum == 0x2)
        call.set_speed(fan::FAN_SPEED_HIGH);
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
  this->fan_->add_on_state_callback([this]() { this->write_state(); });
}

void TuyaFan::dump_config() {
  ESP_LOGCONFIG(TAG, "Tuya Fan:");
  if (this->speed_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Speed has datapoint ID %u", *this->speed_id_);
  if (this->switch_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Switch has datapoint ID %u", *this->switch_id_);
  if (this->oscillation_id_.has_value())
    ESP_LOGCONFIG(TAG, "  Oscillation has datapoint ID %u", *this->oscillation_id_);
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
  if (this->speed_id_.has_value()) {
    TuyaDatapoint datapoint{};
    datapoint.id = *this->speed_id_;
    datapoint.type = TuyaDatapointType::ENUM;
    if (this->fan_->speed == fan::FAN_SPEED_LOW)
      datapoint.value_enum = 0;
    if (this->fan_->speed == fan::FAN_SPEED_MEDIUM)
      datapoint.value_enum = 1;
    if (this->fan_->speed == fan::FAN_SPEED_HIGH)
      datapoint.value_enum = 2;
    ESP_LOGD(TAG, "Setting speed: %d", datapoint.value_enum);
    this->parent_->set_datapoint_value(datapoint);
  }
}

}  // namespace tuya
}  // namespace esphome
