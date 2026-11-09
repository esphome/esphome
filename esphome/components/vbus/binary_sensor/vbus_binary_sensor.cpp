#include "vbus_binary_sensor.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::vbus {

static const char *const TAG = "vbus.binary_sensor";

static void publish_if_set(binary_sensor::BinarySensor *bsensor, bool state) {
  if (bsensor != nullptr)
    bsensor->publish_state(state);
}

void DeltaSolErrorsBSensorBase::warn_short_message_(size_t actual, size_t expected) {
  if (this->warned_short_message_)
    return;
  this->warned_short_message_ = true;
  ESP_LOGW(TAG, "message too short: %zu < %zu", actual, expected);
}

void DeltaSolErrorsBSensorBase::dump_sensor_errors_(const LogString *model) {
  ESP_LOGCONFIG(TAG, "%s:", LOG_STR_ARG(model));
  LOG_BINARY_SENSOR("  ", "Sensor 1 Error", this->s1_error_bsensor_);
  LOG_BINARY_SENSOR("  ", "Sensor 2 Error", this->s2_error_bsensor_);
  LOG_BINARY_SENSOR("  ", "Sensor 3 Error", this->s3_error_bsensor_);
  LOG_BINARY_SENSOR("  ", "Sensor 4 Error", this->s4_error_bsensor_);
}

void DeltaSolErrorsBSensorBase::publish_sensor_errors_(const std::vector<uint8_t> &message, size_t offset) {
  if (!fits(message, offset, 1)) {
    this->warn_short_message_(message.size(), offset + 1);
    return;
  }
  uint8_t errors = message[offset];
  publish_if_set(this->s1_error_bsensor_, errors & 1);
  publish_if_set(this->s2_error_bsensor_, errors & 2);
  publish_if_set(this->s3_error_bsensor_, errors & 4);
  publish_if_set(this->s4_error_bsensor_, errors & 8);
}

void DeltaSolBSPlusBSensor::dump_config() {
  this->dump_sensor_errors_(LOG_STR("Deltasol BS Plus"));
  LOG_BINARY_SENSOR("  ", "Relay 1 On", this->relay1_bsensor_);
  LOG_BINARY_SENSOR("  ", "Relay 2 On", this->relay2_bsensor_);
  LOG_BINARY_SENSOR("  ", "Option Collector Max", this->collector_max_bsensor_);
  LOG_BINARY_SENSOR("  ", "Option Collector Min", this->collector_min_bsensor_);
  LOG_BINARY_SENSOR("  ", "Option Collector Frost", this->collector_frost_bsensor_);
  LOG_BINARY_SENSOR("  ", "Option Tube Collector", this->tube_collector_bsensor_);
  LOG_BINARY_SENSOR("  ", "Option Recooling", this->recooling_bsensor_);
  LOG_BINARY_SENSOR("  ", "Option Heat Quantity Measurement", this->hqm_bsensor_);
}

void DeltaSolBSPlusBSensor::handle_message(std::vector<uint8_t> &message) {
  if (!fits(message, 0, 16))
    this->warn_short_message_(message.size(), 16);
  if (fits(message, 10, 1)) {
    publish_if_set(this->relay1_bsensor_, message[10] & 1);
    publish_if_set(this->relay2_bsensor_, message[10] & 2);
  }
  this->publish_sensor_errors_(message, 11);
  if (fits(message, 15, 1)) {
    publish_if_set(this->collector_max_bsensor_, message[15] & 1);
    publish_if_set(this->collector_min_bsensor_, message[15] & 2);
    publish_if_set(this->collector_frost_bsensor_, message[15] & 4);
    publish_if_set(this->tube_collector_bsensor_, message[15] & 8);
    publish_if_set(this->recooling_bsensor_, message[15] & 0x10);
    publish_if_set(this->hqm_bsensor_, message[15] & 0x20);
  }
}

void DeltaSolBS2009BSensor::dump_config() {
  this->dump_sensor_errors_(LOG_STR("Deltasol BS 2009"));
  LOG_BINARY_SENSOR("  ", "Frost Protection Active", this->frost_protection_active_bsensor_);
}

void DeltaSolBS2009BSensor::handle_message(std::vector<uint8_t> &message) {
  if (!fits(message, 0, 26))
    this->warn_short_message_(message.size(), 26);
  this->publish_sensor_errors_(message, 20);
  if (fits(message, 25, 1))
    publish_if_set(this->frost_protection_active_bsensor_, message[25] & 1);
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
