#include "esphome/core/log.h"
#include "esphome/components/fan/fan_helpers.h"
#include "tuya_fan.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.fan";

void TuyaFan::setup() {
  auto traits = fan::FanTraits(this->oscillation_id_.has_value(), this->speed_id_.has_value(),
                               this->direction_id_.has_value(), this->speed_count_);
  this->fan_->set_traits(traits);

  if (this->speed_id_.has_value()) {
    this->parent_->register_listener(*this->speed_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported speed of: %d", datapoint.value_enum);
      auto call = this->fan_->make_call();
      if (datapoint.value_enum < this->speed_count_)
        call.set_speed(datapoint.value_enum + 1);
      else
        ESP_LOGCONFIG(TAG, "Speed has invalid value %d", datapoint.value_enum);
      call.perform();
    });
  }
  if (this->switch_id_.has_value()) {
    this->parent_->register_listener(*this->switch_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported switch is: %s", ONOFF(datapoint.value_bool));
      auto call = this->fan_->make_call();
      call.set_state(datapoint.value_bool);
      call.perform();
    });
  }
  if (this->oscillation_id_.has_value()) {
    this->parent_->register_listener(*this->oscillation_id_, [this](const TuyaDatapoint &datapoint) {
      ESP_LOGV(TAG, "MCU reported oscillation is: %s", ONOFF(datapoint.value_bool));
      auto call = this->fan_->make_call();
      call.set_oscillating(datapoint.value_bool);
      call.perform();
    });
  }
  if (this->direction_id_.has_value()) {
    this->parent_->register_listener(*this->direction_id_, [this](const TuyaDatapoint &datapoint) {
      auto call = this->fan_->make_call();
      call.set_direction(datapoint.value_bool ? fan::FAN_DIRECTION_REVERSE : fan::FAN_DIRECTION_FORWARD);
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
    ESP_LOGV(TAG, "Setting switch: %s", ONOFF(this->fan_->state));
    this->parent_->set_boolean_datapoint_value(*this->switch_id_, this->fan_->state);
  }
  if (this->oscillation_id_.has_value()) {
    ESP_LOGV(TAG, "Setting oscillating: %s", ONOFF(this->fan_->oscillating));
    this->parent_->set_boolean_datapoint_value(*this->oscillation_id_, this->fan_->oscillating);
  }
  if (this->direction_id_.has_value()) {
    bool enable = this->fan_->direction == fan::FAN_DIRECTION_REVERSE;
    ESP_LOGV(TAG, "Setting reverse direction: %s", ONOFF(enable));
    this->parent_->set_enum_datapoint_value(*this->direction_id_, enable);
  }
  if (this->speed_id_.has_value()) {
    ESP_LOGV(TAG, "Setting speed: %d", this->fan_->speed);
    this->parent_->set_enum_datapoint_value(*this->speed_id_, this->fan_->speed - 1);
  }
}

// We need a higher priority than the FanState component to make sure that the traits are set
// when that component sets itself up.
float TuyaFan::get_setup_priority() const { return fan_->get_setup_priority() + 1.0f; }

}  // namespace tuya
}  // namespace esphome
