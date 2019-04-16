#include "ledc_output.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp32-hal-ledc.h>

namespace esphome {
namespace ledc {

static const char *TAG = "ledc.output";

void LEDCOutput::write_state(float state) {
  if (this->pin_->is_inverted()) {
    state = 1.0f - state;
  }

  const uint32_t max_duty = (uint32_t(1) << this->bit_depth_) - 1;
  const float duty_rounded = roundf(state * max_duty);
  auto duty = static_cast<uint32_t>(duty_rounded);
  ledcWrite(this->channel_, duty);
}

void LEDCOutput::setup() {
  ledcSetup(this->channel_, this->frequency_, this->bit_depth_);
  ledcAttachPin(this->pin_->get_pin(), this->channel_);

  this->turn_off();
}

void LEDCOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "LEDC Output:");
  LOG_PIN("  Pin", this->pin_);
  ESP_LOGCONFIG(TAG, "  LEDC Channel: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  Frequency: %.1f Hz", this->frequency_);
  ESP_LOGCONFIG(TAG, "  Bit Depth: %u", this->bit_depth_);
}

uint8_t next_ledc_channel = 0;

}  // namespace ledc
}  // namespace esphome

#endif
