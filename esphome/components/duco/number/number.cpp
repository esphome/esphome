#include "number.h"

#include <cmath>

namespace esphome::duco {

static constexpr uint8_t PARAMETER_COMFORT_TEMPERATURE = 0x12;
static constexpr uint8_t GROUP_COMFORT_TEMPERATURE = 0x0a;

void DucoComfortTemperature::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, PARAMETER_COMFORT_TEMPERATURE, GROUP_COMFORT_TEMPERATURE});
}

bool DucoComfortTemperature::receive_response(const DucoMessage &message) {
  // The box answers a read and a write with the same message
  if (message.function != FUNCTION_PARAMETER + 2)
    return false;
  // A single byte in tenths of a degree; below 10.0 is not a valid setting
  if (message.size >= 4 && message.data[3] >= 100)
    this->publish_state(message.data[3] / 10.0f);
  return true;
}

void DucoComfortTemperature::control(float value) {
  const auto tenths = static_cast<uint8_t>(lroundf(value * 10.0f));
  this->parent_->send(this, FUNCTION_PARAMETER,
                      {0x01, PARAMETER_COMFORT_TEMPERATURE, GROUP_COMFORT_TEMPERATURE, tenths, 0x00, 0x00, 0x00});
}

}  // namespace esphome::duco
