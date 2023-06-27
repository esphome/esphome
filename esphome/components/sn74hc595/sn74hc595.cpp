#include "sn74hc595.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sn74hc595 {

static const char *const TAG = "sn74hc595";

void SN74HC595Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SN74HC595...");

  if (this->have_oe_pin_) {  // disable output
    this->oe_pin_->setup();
    this->oe_pin_->digital_write(true);
  }

  // initialize output pins
  this->clock_pin_->setup();
  this->data_pin_->setup();
  this->latch_pin_->setup();
  this->clock_pin_->digital_write(false);
  this->data_pin_->digital_write(false);
  this->latch_pin_->digital_write(false);

  // send state to shift register
  this->write_gpio_();
}

void SN74HC595Component::dump_config() { ESP_LOGCONFIG(TAG, "SN74HC595:"); }

void SN74HC595Component::digital_write_(uint16_t pin, bool value) {
  if (pin >= this->sr_count_ * 8) {
    ESP_LOGE(TAG, "Pin %u is out of range! Maximum pin number with %u chips in series is %u", pin, this->sr_count_,
             (this->sr_count_ * 8) - 1);
    return;
  }
  this->output_bits_[pin] = value;
  this->write_gpio_();
}

void SN74HC595Component::write_gpio_() {
  for (auto bit = this->output_bits_.rbegin(); bit != this->output_bits_.rend(); bit++) {
    this->data_pin_->digital_write(*bit);
    this->clock_pin_->digital_write(true);
    this->clock_pin_->digital_write(false);
  }

  // pulse latch to activate new values
  this->latch_pin_->digital_write(true);
  this->latch_pin_->digital_write(false);

  // enable output if configured
  if (this->have_oe_pin_) {
    this->oe_pin_->digital_write(false);
  }
}

float SN74HC595Component::get_setup_priority() const { return setup_priority::IO; }

void SN74HC595GPIOPin::digital_write(bool value) {
  this->parent_->digital_write_(this->pin_, value != this->inverted_);
}
std::string SN74HC595GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via SN74HC595", pin_);
  return buffer;
}

}  // namespace sn74hc595
}  // namespace esphome
