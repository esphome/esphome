#include "esphome/core/log.h"
#include "tuya_number.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.number";

void TuyaNumber::setup() {
  this->parent_->register_listener(this->number_id_, [this](const TuyaDatapoint &datapoint) {
    if (datapoint.type == TuyaDatapointType::INTEGER) {
      ESP_LOGV(TAG, "MCU reported number %u is: %d", datapoint.id, datapoint.value_int);
      this->publish_state(datapoint.value_int);
    } else if (datapoint.type == TuyaDatapointType::ENUM) {
      ESP_LOGV(TAG, "MCU reported number %u is: %u", datapoint.id, datapoint.value_enum);
      this->publish_state(datapoint.value_enum);
    }
    this->type_ = datapoint.type;
  });

  if (this->restore_value_) {
    this->parent_->add_on_initialized_callback([this]() {
      float value;
      this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
      if (this->pref_.load(&value)) {
        ESP_LOGV(TAG, "Restoring number %u: %f", this->number_id_, value);
        if (this->type_ == TuyaDatapointType::INTEGER) {
          this->parent_->set_integer_datapoint_value(this->number_id_, value);
        } else if (this->type_ == TuyaDatapointType::ENUM) {
          this->parent_->set_enum_datapoint_value(this->number_id_, value);
        }
        this->publish_state(value);
      }
    });
  }
}

void TuyaNumber::control(float value) {
  ESP_LOGV(TAG, "Setting number %u: %f", this->number_id_, value);
  if (this->type_ == TuyaDatapointType::INTEGER) {
    this->parent_->set_integer_datapoint_value(this->number_id_, value);
  } else if (this->type_ == TuyaDatapointType::ENUM) {
    this->parent_->set_enum_datapoint_value(this->number_id_, value);
  }
  this->publish_state(value);

  if (this->restore_value_) {
    ESP_LOGV(TAG, "Saving number %u: %f", this->number_id_, value);
    this->pref_.save(&value);
  }
}

void TuyaNumber::dump_config() {
  LOG_NUMBER("", "Tuya Number", this);
  ESP_LOGCONFIG(TAG, "  Number has datapoint ID %u", this->number_id_);
}

}  // namespace tuya
}  // namespace esphome
