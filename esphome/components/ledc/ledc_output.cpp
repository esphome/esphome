#include "ledc_output.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp32-hal-ledc.h>

namespace esphome {
namespace ledc {

static const char *TAG = "ledc.output";

void LEDCOutput::write_state(float state) {
  if (this->pin_->is_inverted())
    state = 1.0f - state;

  this->duty_ = state;
  const uint32_t max_duty = (uint32_t(1) << this->bit_depth_) - 1;
  const float duty_rounded = roundf(state * max_duty);
  auto duty = static_cast<uint32_t>(duty_rounded);
  ledcWrite(this->channel_, duty);
}

void LEDCOutput::setup() {
  this->apply_frequency(this->frequency_);
  this->turn_off();
  // Attach pin after setting default value
  ledcAttachPin(this->pin_->get_pin(), this->channel_);
}

void LEDCOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "LEDC Output:");
  LOG_PIN("  Pin ", this->pin_);
  ESP_LOGCONFIG(TAG, "  LEDC Channel: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  Frequency: %.1f Hz", this->frequency_);
}

float ledc_max_frequency_for_bit_depth(uint8_t bit_depth) { return 80e6f / float(1 << bit_depth); }
float ledc_min_frequency_for_bit_depth(uint8_t bit_depth) {
  const float max_div_num = ((1 << 20) - 1) / 256.0f;
  return 80e6f / (max_div_num * float(1 << bit_depth));
}
optional<uint8_t> ledc_bit_depth_for_frequency(float frequency) {
  for (int i = 20; i >= 1; i--) {
    const float min_frequency = ledc_min_frequency_for_bit_depth(i);
    const float max_frequency = ledc_max_frequency_for_bit_depth(i);
    if (min_frequency <= frequency && frequency <= max_frequency)
      return i;
  }
  return {};
}

void LEDCOutput::apply_frequency(float frequency) {
  auto bit_depth_opt = ledc_bit_depth_for_frequency(frequency);
  if (!bit_depth_opt.has_value()) {
    ESP_LOGW(TAG, "Frequency %f can't be achieved with any bit depth", frequency);
    this->status_set_warning();
  }
  this->bit_depth_ = bit_depth_opt.value_or(8);
  this->frequency_ = frequency;
  ledcSetup(this->channel_, frequency, this->bit_depth_);
  // re-apply duty
  this->write_state(this->duty_);
}

uint8_t next_ledc_channel = 0;

}  // namespace ledc
}  // namespace esphome

#endif
