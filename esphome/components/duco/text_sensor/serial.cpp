#include "serial.h"
#include "../duco.h"
#include <vector>

namespace esphome {
namespace duco {

static const char *const TAG = "duco sensor";

void DucoSerial::setup() {}

void DucoSerial::update() {
  DucoMessage message;
  message.function = 0x10;
  message.data = {0x01, 0x01, 0x00, 0x1a, 0x10};
  this->parent_->send(message, this);
}

float DucoSerial::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

void DucoSerial::receive_response(const DucoMessage &message) {
  if (message.function == 0x12) {
    // Serial response received, parse it
    std::string serial(message.data.begin() + 2, message.data.end());
    ESP_LOGD(TAG, "Box Serial: %s", serial.c_str());

    publish_state(serial);

    // do not wait for new messages with the same ID
    this->parent_->stop_waiting(message.id);
  }
}

}  // namespace duco
}  // namespace esphome
