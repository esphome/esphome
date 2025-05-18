#include "sensor.h"
#include "../duco.h"
#include <vector>

namespace esphome {
namespace duco {

static const char *const TAG = "duco sensor";

void DucoCo2Sensor::setup() {}

void DucoCo2Sensor::update() {
  DucoMessage message;
  message.function = 0x10;
  message.data = {0x01, address_, 0x00, 0x49, 0x04};
  this->parent_->send(message, this);
}

float DucoCo2Sensor::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

void DucoCo2Sensor::receive_response(const DucoMessage &message) {
  if (message.function == 0x12) {
    uint16_t co2_value = (message.data[5] << 8) + message.data[4];
    // only publish the state if the co2 value is below 10000
    // otherwise the value is likely invalid
    if (co2_value <= 10000)
      publish_state(co2_value);

    this->parent_->stop_waiting(message.id);
  }
}

void DucoCo2Sensor::set_address(uint8_t address) { this->address_ = address; }

void DucoHumiditySensor::setup() {}

void DucoHumiditySensor::update() {
  DucoMessage message;
  message.function = 0x10;
  message.data = {0x01, address_, 0x00, 0x49, 0x04};
  this->parent_->send(message, this);
}

float DucoHumiditySensor::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

void DucoHumiditySensor::receive_response(const DucoMessage &message) {
  if (message.function == 0x12) {
    uint16_t rh_value = (message.data[7] << 8) + message.data[6];
    // only publish the state if the co2 value is below 10000
    // otherwise the value is likely invalid
    if (rh_value <= 10000)
      publish_state(rh_value / 100.0);

    this->parent_->stop_waiting(message.id);
  }
}

void DucoHumiditySensor::set_address(uint8_t address) { this->address_ = address; }

void DucoTemperatureSensor::setup() {}

void DucoTemperatureSensor::update() {
  DucoMessage message;
  message.function = 0x10;
  message.data = {0x01, address_, 0x00, 0x49, 0x04};
  this->parent_->send(message, this);
}

float DucoTemperatureSensor::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

void DucoTemperatureSensor::receive_response(const DucoMessage &message) {
  if (message.function == 0x12) {
    uint16_t temp_value = (message.data[3] << 8) + message.data[2];
    // only publish the state if the co2 value is below 10000
    // otherwise the value is likely invalid
    if (temp_value <= 1000)
      publish_state(temp_value / 10.0);

    this->parent_->stop_waiting(message.id);
  }
}

void DucoTemperatureSensor::set_address(uint8_t address) { this->address_ = address; }

void DucoBoxTemperatureSensor::setup() {}

void DucoBoxTemperatureSensor::update() {
  DucoMessage message;
  message.function = 0x24;
  message.data = {0x00, type_, 0x09};
  this->parent_->send(message, this);
}

float DucoBoxTemperatureSensor::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

void DucoBoxTemperatureSensor::receive_response(const DucoMessage &message) {
  if (message.function == 0x26) {
    uint16_t temp_value = (message.data[4] << 8) + message.data[3];
    // only publish the state if the co2 value is below 10000
    // otherwise the value is likely invalid
    if (temp_value <= 1000)
      publish_state(temp_value / 10.0);

    this->parent_->stop_waiting(message.id);
  }
}

void DucoBoxTemperatureSensor::set_type(uint8_t type) { this->type_ = type; }

void DucoBypassSensor::setup() {}

void DucoBypassSensor::update() {
  DucoMessage message;
  message.function = 0x24;
  message.data = {0x00, 0x10, 0x09};
  this->parent_->send(message, this);
}

float DucoBypassSensor::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

void DucoBypassSensor::receive_response(const DucoMessage &message) {
  if (message.function == 0x26) {
    uint16_t bypass_value = message.data[3];
    // only publish the state if the co2 value is below 10000
    // otherwise the value is likely invalid
    if (bypass_value <= 100)
      publish_state(bypass_value);

    this->parent_->stop_waiting(message.id);
  }
}

void DucoFilterRemainingSensor::setup() {}

void DucoFilterRemainingSensor::update() {
  DucoMessage message;
  message.function = 0x24;
  message.data = {0x00, 0x30, 0x09};
  this->parent_->send(message, this);
}

float DucoFilterRemainingSensor::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

void DucoFilterRemainingSensor::receive_response(const DucoMessage &message) {
  if (message.function == 0x26) {
    uint8_t filter_remaining = message.data[3];
    publish_state(filter_remaining);

    this->parent_->stop_waiting(message.id);
  }
}

void DucoFlowLevelSensor::setup() {}

void DucoFlowLevelSensor::update() {
  DucoMessage message;
  message.function = 0x0c;
  message.data = {0x02, 0x01};
  this->parent_->send(message, this);
}

float DucoFlowLevelSensor::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

void DucoFlowLevelSensor::receive_response(const DucoMessage &message) {
  if (message.function == 0x0e) {
    uint8_t flow_level = message.data[2];
    publish_state(flow_level);

    this->parent_->stop_waiting(message.id);
  }
}

void DucoStateTimeRemainingSensor::setup() {}

void DucoStateTimeRemainingSensor::update() {
  DucoMessage message;
  message.function = 0x0c;
  message.data = {0x02, 0x01};
  this->parent_->send(message, this);
}

float DucoStateTimeRemainingSensor::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

void DucoStateTimeRemainingSensor::receive_response(const DucoMessage &message) {
  if (message.function == 0x0e) {
    uint16_t time_remaining = (message.data[13] << 8) + message.data[12];
    publish_state(time_remaining);

    this->parent_->stop_waiting(message.id);
  }
}

}  // namespace duco
}  // namespace esphome
