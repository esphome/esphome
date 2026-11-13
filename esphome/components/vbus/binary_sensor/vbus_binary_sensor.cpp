#include "vbus_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::vbus {

ESPHOME_LOG_TAG(TAG, "vbus.binary_sensor");

void DeltaSolErrorsBSensorBase::warn_short_message_(size_t actual, size_t expected) {
  if (this->warned_short_message_)
    return;
  this->warned_short_message_ = true;
  ESP_LOGW(TAG, "message too short: %zu < %zu", actual, expected);
}

void DeltaSolErrorsBSensorBase::dump_sensor_errors_(const LogString *model) {
  ESP_LOGCONFIG(TAG, "%s:", LOG_STR_ARG(model));
  LOG_BINARY_SENSOR("  ", "Sensor 1 Error", this->s1_error_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Sensor 2 Error", this->s2_error_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Sensor 3 Error", this->s3_error_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Sensor 4 Error", this->s4_error_binary_sensor_);
}

void DeltaSolErrorsBSensorBase::publish_sensor_errors_(const std::vector<uint8_t> &message, size_t offset) {
  if (!fits(message, offset, 1)) {
    this->warn_short_message_(message.size(), offset + 1);
    return;
  }
  uint8_t errors = message[offset];
  PUBLISH_STATE(this->s1_error_binary_sensor_, errors & 1);
  PUBLISH_STATE(this->s2_error_binary_sensor_, errors & 2);
  PUBLISH_STATE(this->s3_error_binary_sensor_, errors & 4);
  PUBLISH_STATE(this->s4_error_binary_sensor_, errors & 8);
}

void DeltaSolBSPlusBSensor::dump_config() {
  this->dump_sensor_errors_(LOG_STR("Deltasol BS Plus"));
  LOG_BINARY_SENSOR("  ", "Relay 1 On", this->relay1_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Relay 2 On", this->relay2_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Option Collector Max", this->collector_max_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Option Collector Min", this->collector_min_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Option Collector Frost", this->collector_frost_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Option Tube Collector", this->tube_collector_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Option Recooling", this->recooling_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Option Heat Quantity Measurement", this->hqm_binary_sensor_);
}

void DeltaSolBSPlusBSensor::handle_message(std::vector<uint8_t> &message) {
  if (!fits(message, 0, 16))
    this->warn_short_message_(message.size(), 16);
  if (fits(message, 10, 1)) {
    PUBLISH_STATE(this->relay1_binary_sensor_, message[10] & 1);
    PUBLISH_STATE(this->relay2_binary_sensor_, message[10] & 2);
  }
  this->publish_sensor_errors_(message, 11);
  if (fits(message, 15, 1)) {
    PUBLISH_STATE(this->collector_max_binary_sensor_, message[15] & 1);
    PUBLISH_STATE(this->collector_min_binary_sensor_, message[15] & 2);
    PUBLISH_STATE(this->collector_frost_binary_sensor_, message[15] & 4);
    PUBLISH_STATE(this->tube_collector_binary_sensor_, message[15] & 8);
    PUBLISH_STATE(this->recooling_binary_sensor_, message[15] & 0x10);
    PUBLISH_STATE(this->hqm_binary_sensor_, message[15] & 0x20);
  }
}

void DeltaSolBS2009BSensor::dump_config() {
  this->dump_sensor_errors_(LOG_STR("Deltasol BS 2009"));
  LOG_BINARY_SENSOR("  ", "Frost Protection Active", this->frost_protection_active_binary_sensor_);
}

void DeltaSolBS2009BSensor::handle_message(std::vector<uint8_t> &message) {
  if (!fits(message, 0, 26))
    this->warn_short_message_(message.size(), 26);
  this->publish_sensor_errors_(message, 20);
  if (fits(message, 25, 1))
    PUBLISH_STATE(this->frost_protection_active_binary_sensor_, message[25] & 1);
}

void VBusCustomBSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "VBus Custom Binary Sensor:");
  if (this->source_ == 0xffff) {
    ESP_LOGCONFIG(TAG, "  Source address: ANY");
  } else {
    ESP_LOGCONFIG(TAG, "  Source address: 0x%04x", this->source_);
  }
  if (this->dest_ == 0xffff) {
    ESP_LOGCONFIG(TAG, "  Dest address: ANY");
  } else {
    ESP_LOGCONFIG(TAG, "  Dest address: 0x%04x", this->dest_);
  }
  if (this->command_ == 0xffff) {
    ESP_LOGCONFIG(TAG, "  Command: ANY");
  } else {
    ESP_LOGCONFIG(TAG, "  Command: 0x%04x", this->command_);
  }
  ESP_LOGCONFIG(TAG, "  Binary Sensors:");
  for (VBusCustomSubBSensor *bsensor : this->bsensors_) {
    LOG_BINARY_SENSOR("  ", "-", bsensor);
  }
}

void VBusCustomBSensor::handle_message(std::vector<uint8_t> &message) {
  for (VBusCustomSubBSensor *bsensor : this->bsensors_)
    bsensor->parse_message(message);
}

void VBusCustomSubBSensor::parse_message(std::vector<uint8_t> &message) {
  this->publish_state(this->message_parser_(message));
}

}  // namespace esphome::vbus
