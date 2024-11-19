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

void DucoCo2Sensor::receive_response(std::vector<uint8_t> message) {
  if (message[1] == 0x12) {
    uint16_t co2_value = (message[8] << 8) + message[7];
    publish_state(co2_value);

    this->parent_->stop_waiting(message[2]);
  }
}

void DucoCo2Sensor::set_address(uint8_t address) { this->address_ = address; }

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

void DucoFilterRemainingSensor::receive_response(std::vector<uint8_t> message) {
  if (message[1] == 0x26) {
    uint8_t filter_remaining = message[6];
    publish_state(filter_remaining);

    this->parent_->stop_waiting(message[2]);
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

void DucoFlowLevelSensor::receive_response(std::vector<uint8_t> message) {
  if (message[1] == 0x0e) {
    uint8_t flow_level = message[5];
    publish_state(flow_level);

    this->parent_->stop_waiting(message[2]);
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

void DucoStateTimeRemainingSensor::receive_response(std::vector<uint8_t> message) {
  if (message[1] == 0x0e) {
    uint16_t time_remaining = (message[16] << 8) + message[15];
    publish_state(time_remaining);

    this->parent_->stop_waiting(message[2]);
  }
}

}  // namespace duco
}  // namespace esphome
