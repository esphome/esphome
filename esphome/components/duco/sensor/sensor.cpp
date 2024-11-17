#include "sensor.h"
#include "../duco.h"
#include <vector>

namespace esphome {
namespace duco {

static const char *const TAG = "duco sensor";

void DucoCo2Sensor::setup() {}

void DucoCo2Sensor::update() {
  // ask for current mode
  ESP_LOGD(TAG, "CO2: Get sensor information");

  std::vector<uint8_t> message = {0x01, address_, 0x00, 0x49, 0x04};
  this->parent_->send(0x10, message, this);
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

}  // namespace duco
}  // namespace esphome
