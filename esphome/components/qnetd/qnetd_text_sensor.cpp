#include "esphome/core/defines.h"
#ifdef USE_TEXT_SENSOR
#include "qnetd_text_sensor.h"

#include "esphome/core/log.h"

namespace esphome::qnetd {

ESPHOME_LOG_TAG(TAG, "qnetd.text_sensor");

void QnetdStatusTextSensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->publish_(); });
  this->publish_();
}

void QnetdStatusTextSensor::publish_() {
  char status[256];
  this->parent_->status_to(status);
  // the callback fires on any change; only republish when the summary changed
  if (!this->has_state() || this->state != status)
    this->publish_state(status);
}

void QnetdStatusTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Qnetd Status", this); }

}  // namespace esphome::qnetd
#endif  // USE_TEXT_SENSOR
