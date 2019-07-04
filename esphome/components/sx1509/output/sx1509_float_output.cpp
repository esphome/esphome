#include "sx1509_float_output.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sx1509 {

static const char *TAG = "sx1509_float_channel";

void SX1509FloatOutputChannel::write_state(float state) {
  const uint16_t max_duty = 255;
  const float duty_rounded = roundf(state * max_duty);
  auto duty = static_cast<uint16_t>(duty_rounded);
  this->parent_->set_pin_value(this->pin_, duty);
}

void SX1509FloatOutputChannel::setup() {
  ESP_LOGD(TAG, "setup pin %d", this->pin_);
  this->parent_->pin_mode(this->pin_, SX1509_ANALOG_OUTPUT);
  this->turn_off();
}

void SX1509FloatOutputChannel::dump_config() {
  ESP_LOGCONFIG(TAG, "SX1509 PWM:");
  ESP_LOGCONFIG(TAG, "  sx1509 pin: %d", this->pin_);
  LOG_FLOAT_OUTPUT(this);
}

}  // namespace sx1509
}  // namespace esphome
