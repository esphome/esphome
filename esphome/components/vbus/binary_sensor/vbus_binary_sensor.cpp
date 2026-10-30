#include "vbus_binary_sensor.h"
#include "esphome/core/helpers.h"

namespace esphome::vbus {

void DeltaSolBSPlusBSensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->relay1_binary_sensor_, message[10] & 1);
  PUBLISH_STATE(this->relay2_binary_sensor_, message[10] & 2);
  PUBLISH_STATE(this->s1_error_binary_sensor_, message[11] & 1);
  PUBLISH_STATE(this->s2_error_binary_sensor_, message[11] & 2);
  PUBLISH_STATE(this->s3_error_binary_sensor_, message[11] & 4);
  PUBLISH_STATE(this->s4_error_binary_sensor_, message[11] & 8);
  PUBLISH_STATE(this->collector_max_binary_sensor_, message[15] & 1);
  PUBLISH_STATE(this->collector_min_binary_sensor_, message[15] & 2);
  PUBLISH_STATE(this->collector_frost_binary_sensor_, message[15] & 4);
  PUBLISH_STATE(this->tube_collector_binary_sensor_, message[15] & 8);
  PUBLISH_STATE(this->recooling_binary_sensor_, message[15] & 0x10);
  PUBLISH_STATE(this->hqm_binary_sensor_, message[15] & 0x20);
}

void DeltaSolBS2009BSensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->s1_error_binary_sensor_, message[20] & 1);
  PUBLISH_STATE(this->s2_error_binary_sensor_, message[20] & 2);
  PUBLISH_STATE(this->s3_error_binary_sensor_, message[20] & 4);
  PUBLISH_STATE(this->s4_error_binary_sensor_, message[20] & 8);
  PUBLISH_STATE(this->frost_protection_active_binary_sensor_, message[25] & 1);
}

void DeltaSolCBSensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->s1_error_binary_sensor_, message[10] & 1);
  PUBLISH_STATE(this->s2_error_binary_sensor_, message[10] & 2);
  PUBLISH_STATE(this->s3_error_binary_sensor_, message[10] & 4);
  PUBLISH_STATE(this->s4_error_binary_sensor_, message[10] & 8);
}

void DeltaSolCS2BSensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->s1_error_binary_sensor_, message[18] & 1);
  PUBLISH_STATE(this->s2_error_binary_sensor_, message[18] & 2);
  PUBLISH_STATE(this->s3_error_binary_sensor_, message[18] & 4);
  PUBLISH_STATE(this->s4_error_binary_sensor_, message[18] & 8);
}

void DeltaSolCS4BSensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->s1_error_binary_sensor_, message[20] & 1);
  PUBLISH_STATE(this->s2_error_binary_sensor_, message[20] & 2);
  PUBLISH_STATE(this->s3_error_binary_sensor_, message[20] & 4);
  PUBLISH_STATE(this->s4_error_binary_sensor_, message[20] & 8);
}

void DeltaSolCSPlusBSensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->s1_error_binary_sensor_, message[20] & 1);
  PUBLISH_STATE(this->s2_error_binary_sensor_, message[20] & 2);
  PUBLISH_STATE(this->s3_error_binary_sensor_, message[20] & 4);
  PUBLISH_STATE(this->s4_error_binary_sensor_, message[20] & 8);
}

void DeltaSolBS2BSensor::handle_message(std::vector<uint8_t> &message) {
  PUBLISH_STATE(this->s1_error_binary_sensor_, message[10] & 1);
  PUBLISH_STATE(this->s2_error_binary_sensor_, message[10] & 2);
  PUBLISH_STATE(this->s3_error_binary_sensor_, message[10] & 4);
  PUBLISH_STATE(this->s4_error_binary_sensor_, message[10] & 8);
}

void VBusCustomBSensor::handle_message(std::vector<uint8_t> &message) {
  for (VBusCustomSubBSensor *bsensor : this->bsensors_)
    bsensor->parse_message(message);
}

void VBusCustomSubBSensor::parse_message(std::vector<uint8_t> &message) {
  this->publish_state(this->message_parser_(message));
}

}  // namespace esphome::vbus
