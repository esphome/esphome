#include "esphome/core/log.h"
#include "tuya_number.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.number";

void TuyaNumber::setup() {
  this->parent_->register_listener(this->number_id_, [this](const TuyaDatapoint &datapoint) {
    if (datapoint.type == TuyaDatapointType::INTEGER) {
      ESP_LOGV(TAG, "MCU reported number %u is: %d", datapoint.id, datapoint.value_int);
      this->publish_state(datapoint.value_int / multiply_by_);
    } else if (datapoint.type == TuyaDatapointType::ENUM) {
      ESP_LOGV(TAG, "MCU reported number %u is: %u", datapoint.id, datapoint.value_enum);
      this->publish_state(datapoint.value_enum);
    }
    if ((this->type_) && (this->type_ != datapoint.type))
    {
       ESP_LOGW(TAG, "Reported type (%d) different than previously set (%d)!", datapoint.type, this->type_);
    }
    this->type_ = datapoint.type;
  });

  this->parent_->add_on_initialized_callback([this]{
    if ((this->value_to_restore_) && (this->type_))
    {
      this->control(*this->value_to_restore_);
    }
  });
}

void TuyaNumber::control(float value) {
  ESP_LOGV(TAG, "Setting number %u: %f", this->number_id_, value);
  if (this->type_ == TuyaDatapointType::INTEGER) {
    int integer_value = lround(value * multiply_by_);
    this->parent_->set_integer_datapoint_value(this->number_id_, integer_value);
  } else if (this->type_ == TuyaDatapointType::ENUM) {
    this->parent_->set_enum_datapoint_value(this->number_id_, value);
  }
  this->publish_state(value);
}

void TuyaNumber::dump_config() {
  LOG_NUMBER("", "Tuya Number", this);
  ESP_LOGCONFIG(TAG, "  Number has datapoint ID %u", this->number_id_);
  if (this->type_)
  {
    ESP_LOGCONFIG(TAG, "  Datapoint type is %d", *this->type_);
  }
  else
  {
    ESP_LOGCONFIG(TAG, "  Datapoint type is unknown");
  }

  if (this->value_to_restore_)
  {
    ESP_LOGCONFIG(TAG, "  Value to restore is %f", *this->value_to_restore_);
  }
}

}  // namespace tuya
}  // namespace esphome
