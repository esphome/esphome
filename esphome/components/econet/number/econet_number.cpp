#include "econet_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace econet {

static const char *const TAG = "econet.number";

void EconetNumber::setup() {
  this->parent_->register_listener(
      this->number_id_, this->request_mod_, this->request_once_, [this](const EconetDatapoint &datapoint) {
        if (datapoint.type == EconetDatapointType::FLOAT) {
          ESP_LOGV(TAG, "MCU reported number %s is: %f", this->number_id_.c_str(), datapoint.value_float);
          this->publish_state(datapoint.value_float);
        } else if (datapoint.type == EconetDatapointType::ENUM_TEXT) {
          ESP_LOGV(TAG, "MCU reported number %s is: %u", this->number_id_.c_str(), datapoint.value_enum);
          this->publish_state(datapoint.value_enum);
        }
        this->type_ = datapoint.type;
      });
}

void EconetNumber::control(float value) {
  ESP_LOGV(TAG, "Setting number %s: %f", this->number_id_.c_str(), value);
  if (this->type_ == EconetDatapointType::FLOAT) {
    this->parent_->set_float_datapoint_value(this->number_id_, value);
  } else if (this->type_ == EconetDatapointType::ENUM_TEXT) {
    this->parent_->set_enum_datapoint_value(this->number_id_, value);
  }
  this->publish_state(value);
}

void EconetNumber::dump_config() {
  LOG_NUMBER("", "Econet Number", this);
  ESP_LOGCONFIG(TAG, "  Number has datapoint ID %s", this->number_id_.c_str());
}

}  // namespace econet
}  // namespace esphome
