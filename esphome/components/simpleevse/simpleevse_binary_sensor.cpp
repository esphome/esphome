#include "simpleevse_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace simpleevse {

void SimpleEvseBinarySensors::update(bool running, const std::array<uint16_t, COUNT_STATUS_REGISTER> &status_register)
{
  if (this->connected_) {
    this->connected_->publish_state(running);
  }
}

}  // namespace simpleevse
}  // namespace esphome
