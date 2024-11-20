#include "number.h"
#include "../duco.h"
#include <vector>

namespace esphome {
namespace duco {

static const char *const TAG = "duco number";

void DucoComfortTemperature::setup() {}

void DucoComfortTemperature::update() {
  DucoMessage message;
  message.function = 0x24;
  message.data = {0x00, 0x12, 0x0a};
  this->parent_->send(message, this);
}

float DucoComfortTemperature::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

void DucoComfortTemperature::receive_response(DucoMessage message) {
  // the DUCO box responds with the same message, both for reading and setting the comfort temperature
  if (message.function == 0x26) {
    publish_state(message.data[3] / 10.0);

    this->parent_->stop_waiting(message.id);
  }
}

void DucoComfortTemperature::control(float number) {
  uint8_t temperature = number * 10;

  DucoMessage message;
  message.function = 0x24;
  message.data = {0x01, 0x12, 0x0a, temperature, 0x00, 0x00, 0x00};
  this->parent_->send(message, this);
}

}  // namespace duco
}  // namespace esphome
