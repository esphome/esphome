#include "hybridshiftreg.h"
#include "esphome/core/log.h"

namespace esphome {
namespace hybridshiftreg {

static const char *const TAG = "hybridshiftreg";

void HybridShiftComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HybridShiftRegister...");

  this->load_oe_pin_->setup();
  this->load_oe_pin_->digital_write(true);

  // initialize output pins
  this->clock_pin_->setup();
  this->data_out_pin_->setup();
  this->data_in_pin_->setup();
  this->inh_latch_pin_->setup();
  this->clock_pin_->digital_write(false);
  this->data_out_pin_->digital_write(false);
  this->inh_latch_pin_->digital_write(false);

  // send state to shift register
  this->read_write_gpio_();
}

void HybridShiftComponent::dump_config() { ESP_LOGCONFIG(TAG, "HybridShiftRegister:"); }

void HybridShiftComponent::digital_write_(uint16_t pin, bool value) {
  if (pin >= this->sr_count_ * 8) {
    ESP_LOGE(TAG, "Pin %u is out of range! Maximum pin number with %u chips in series is %u", pin, this->sr_count_,
             (this->sr_count_ * 8) - 1);
    return;
  }
  this->output_bits_[pin] = value;
}

bool HybridShiftComponent::digital_read_(uint16_t pin) {
  if (pin >= this->sr_count_ * 8) {
    ESP_LOGE(TAG, "Pin %u is out of range! Maximum pin number with %u chips in series is %u", pin, this->sr_count_,
             (this->sr_count_ * 8) - 1);
    return false;
  }
  return this->input_bits_[pin];
}

void HybridShiftComponent::loop() { this->read_write_gpio_(); }

void HybridShiftComponent::read_write_gpio_() {
  for (uint8_t i = 0; i < this->sr_count_; i++) {
    for (uint8_t j = 0; j < 8; j++) {
      this->data_out_pin_->digital_write(output_bits_[(i * 8) + (7 - j)]);
      this->input_bits_[(i * 8) + (7 - j)] = this->data_in_pin_->digital_read();

      this->clock_pin_->digital_write(true);
      delayMicroseconds(10);
      this->clock_pin_->digital_write(false);
      delayMicroseconds(10);
    }
  }

  // pulse latch to activate new values
  this->inh_latch_pin_->digital_write(true);
  this->inh_latch_pin_->digital_write(false);

  this->load_oe_pin_->digital_write(false);
}

float HybridShiftComponent::get_setup_priority() const { return setup_priority::IO; }

void HybridShiftGPIOPin::digital_write(bool value) {
  this->parent_->digital_write_(this->pin_, value != this->inverted_);
}
bool HybridShiftGPIOPin::digital_read() { return this->parent_->digital_read_(this->pin_) != this->inverted_; }
std::string HybridShiftGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via HybridShiftReg", pin_);
  return buffer;
}

}  // namespace hybridshiftreg
}  // namespace esphome
