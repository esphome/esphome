#include "number.h"

namespace esphome::duco {

static constexpr uint8_t PARAMETER_COMFORT_TEMPERATURE = 0x12;
static constexpr uint8_t GROUP_COMFORT_TEMPERATURE = 0x0a;

void DucoComfortTemperature::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, PARAMETER_COMFORT_TEMPERATURE, GROUP_COMFORT_TEMPERATURE});
}

void DucoComfortTemperature::receive_response(const DucoMessage &message) {
  // Reads and writes get the same answer: one byte in tenths of a degree, below 10.0 is invalid
  if (message.size >= 4 && message.data[3] >= 100)
    this->publish_state(message.data[3] / 10.0f);
}

void DucoComfortTemperature::control(float value) {
  const auto tenths = static_cast<uint8_t>(value * 10.0f + 0.5f);
  this->parent_->send(this, FUNCTION_PARAMETER,
                      {0x01, PARAMETER_COMFORT_TEMPERATURE, GROUP_COMFORT_TEMPERATURE, tenths, 0x00, 0x00, 0x00});
}

}  // namespace esphome::duco
