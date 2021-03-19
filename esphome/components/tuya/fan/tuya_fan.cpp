#include "esphome/core/log.h"
#include "esphome/components/fan/fan_helpers.h"
#include "tuya_fan.h"

namespace esphome {
namespace tuya {

static const char *TAG = "tuya.fan";

void TuyaFan::setup() {
  auto traits = fan::FanTraits(this->oscillation_id_.has_value(), this->speed_id_.has_value(), false, 3);
  this->fan_->set_traits(traits);

  if (this->speed_id_.has_value()) {
    this->parent_->register_listener(*this->speed_id_, [this](TuyaDatapoint datapoint) {
      ESP_LOGV(TAG, "MCU reported speed of: %d", datapoint.value_enum);
      if (datapoint.value_enum > 0x02) {
        ESP_LOGE(TAG, "Speed has invalid value %d", datapoint.value_enum);
        return;
      }
      auto call = this->fan_->make_call();
      call.set_speed(datapoint.value_enum);
      call.perform();
    });
  }
  if (this->switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](TuyaDatapoint datapoint) {
      ESP_LOGV(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
      auto call = this->fan_->make_call();
      call.set_state(datapoint.value_bool);
      call.perform();
    });
  }
  if (this->oscillation_id_.has_value()) {
    this->parent_->register_listener(*this->oscillation_id_, [this](TuyaDatapoint datapoint) {
      ESP_LOGV(TAG, "MCU reported oscillation is: %s", ONOFF(datapoint.value_bool));
      auto call = this->fan_->make_call();
      call.set_oscillating(datapoint.value_bool);
      call.perform();
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
    ESP_LOGV(TAG, "Setting switch: %s", ONOFF(this->fan_->state));
    this->parent_->set_datapoint_value(*this->switch_id_, this->fan_->state);
  }
  if (this->oscillation_id_.has_value()) {
    ESP_LOGV(TAG, "Setting oscillating: %s", ONOFF(this->fan_->oscillating));
    this->parent_->set_datapoint_value(*this->oscillation_id_, this->fan_->oscillating);
  }
  if (this->speed_id_.has_value()) {
    ESP_LOGV(TAG, "Setting speed: %d", this->fan_->speed);
    this->parent_->set_datapoint_value(*this->speed_id_, this->fan_->speed);
  }
}

}  // namespace tuya
}  // namespace esphome
