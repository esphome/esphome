#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "qnetd_binary_sensor.h"

#include "esphome/core/log.h"

namespace esphome::qnetd {

ESPHOME_LOG_TAG(TAG, "qnetd.binary_sensor");

void QnetdVoteGrantedBinarySensor::setup() {
  this->parent_->add_on_state_callback([this]() { this->publish_state(this->parent_->vote_granted()); });
  this->publish_state(this->parent_->vote_granted());
}

void QnetdVoteGrantedBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Qnetd Vote Granted", this); }

}  // namespace esphome::qnetd
#endif  // USE_BINARY_SENSOR
