#include "sx1509_float_output.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sx1509 {

static const char *TAG = "sx1509";

void SX1509FloatOutputChannel::write_state(float state) {
  ESP_LOGD(TAG, "write_state %f", state);
  const uint16_t max_duty = 255;
  const float duty_rounded = roundf(state * max_duty);
  auto duty = static_cast<uint16_t>(duty_rounded);
  this->parent_->set_pin_value_(this->pin_, duty);
}

void SX1509FloatOutputChannel::setup_channel() { this->parent_->pin_mode(this->pin_, ANALOG_OUTPUT); }

}  // namespace sx1509
}  // namespace esphome