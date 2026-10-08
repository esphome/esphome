#include "esphome/core/defines.h"
#ifdef USE_SENSOR
#include "qnetd_sensor.h"

#include "esphome/core/log.h"

namespace esphome::qnetd {

ESPHOME_LOG_TAG(TAG, "qnetd.sensor");

void QnetdSensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->publish_(); });
  this->publish_();
}

void QnetdSensor::publish_() {
  if (this->type_ == QnetdSensorType::QNETD_SENSOR_TYPE_CONNECTED_CLIENTS) {
    this->publish_state(this->parent_->connected_clients());
  } else {
    this->publish_state(this->parent_->decisions());
  }
}

void QnetdSensor::dump_config() { LOG_SENSOR("", "Qnetd Sensor", this); }

}  // namespace esphome::qnetd
#endif  // USE_SENSOR
