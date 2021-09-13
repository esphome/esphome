#include "esphome/core/log.h"
#include "tuya_number.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.number";

void TuyaNumber::setup() {
  this->parent_->register_listener(this->number_id_, [this](const TuyaDatapoint &datapoint) {
    ESP_LOGV(TAG, "MCU reported number %u is: %d", this->number_id_, datapoint.value_int);
    this->publish_state(datapoint.value_int);
  });
}

void TuyaNumber::control(float value) {
  ESP_LOGV(TAG, "Setting number %u: %s", this->number_id_, value);
  this->parent_->set_integer_datapoint_value(this->number_id_, value);
  this->publish_state(value);
}

void TuyaNumber::dump_config() {
  LOG_NUMBER("", "Tuya Number", this);
  ESP_LOGCONFIG(TAG, "  Number has datapoint ID %u", this->number_id_);
}

}  // namespace tuya
}  // namespace esphome
