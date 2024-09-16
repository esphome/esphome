#include "esphome/core/log.h"
#include "tuya_number.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.number";

void TuyaNumber::setup() {
  if (this->restore_value_) {
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  }

  this->parent_->register_listener(this->number_id_, [this](const TuyaDatapoint &datapoint) {
    if (datapoint.type == TuyaDatapointType::INTEGER) {
      ESP_LOGV(TAG, "MCU reported number %u is: %d", datapoint.id, datapoint.value_int);
      float value = datapoint.value_int / multiply_by_;
      this->publish_state(value);
      if (this->restore_value_)
        this->pref_.save(&value);
    } else if (datapoint.type == TuyaDatapointType::ENUM) {
      ESP_LOGV(TAG, "MCU reported number %u is: %u", datapoint.id, datapoint.value_enum);
      float value = datapoint.value_enum;
      this->publish_state(value);
      if (this->restore_value_)
        this->pref_.save(&value);
    } else {
      ESP_LOGW(TAG, "Reported type (%d) is not a number!", static_cast<int>(datapoint.type));
      return;
    }

    if ((this->type_) && (this->type_ != datapoint.type)) {
      ESP_LOGW(TAG, "Reported type (%d) different than previously set (%d)!", static_cast<int>(datapoint.type),
               static_cast<int>(*this->type_));
    }
    this->type_ = datapoint.type;
  });

  this->parent_->add_on_initialized_callback([this] {
    if (this->type_) {
      float value;
      if (!this->restore_value_) {
        if (this->initial_value_) {
          value = *this->initial_value_;
        } else {
          return;
        }
      } else {
        if (!this->pref_.load(&value)) {
          if (this->initial_value_) {
            value = *this->initial_value_;
          } else {
            value = this->traits.get_min_value();
            ESP_LOGW(TAG, "Failed to restore and there is no initial value defined. Setting min_value (%f)", value);
          }
        }
      }

      this->control(value);
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

  if (this->restore_value_)
    this->pref_.save(&value);
}

void TuyaNumber::dump_config() {
  LOG_NUMBER("", "Tuya Number", this);
  ESP_LOGCONFIG(TAG, "  Number has datapoint ID %u", this->number_id_);
  if (this->type_) {
    ESP_LOGCONFIG(TAG, "  Datapoint type is %d", static_cast<int>(*this->type_));
  } else {
    ESP_LOGCONFIG(TAG, "  Datapoint type is unknown");
  }

  if (this->initial_value_) {
    ESP_LOGCONFIG(TAG, "  Initial Value: %f", *this->initial_value_);
  }

  ESP_LOGCONFIG(TAG, "  Restore Value: %s", YESNO(this->restore_value_));
}

}  // namespace tuya
}  // namespace esphome
