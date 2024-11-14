#include "sensor.h"
#include "../duco.h"
#include <vector>

namespace esphome {
namespace duco {

static const char *const TAG = "duco sensor";

void DucoSensor::setup() {}

void DucoSensor::update() {
  // ask for serial
  std::vector<uint8_t> message = {0x01, 0x01, 0x00, 0x1a, 0x10};
  this->parent_->send(0x10, message, this);
}

float DucoSensor::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

void DucoSensor::receive_response(std::vector<uint8_t> message) {
  if (message[1] == 0x12) {
    // Serial response received, parse it
    std::string serial(message.begin() + 5, message.end());
    ESP_LOGD(TAG, "Box Serial: %s", serial.c_str());

    publish_state(serial);

    // do not wait for new messages with the same ID
    this->parent_->stop_waiting(message[2]);
  }
}

}  // namespace duco
}  // namespace esphome
