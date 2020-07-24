#include "sn74hc595.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sn74hc595 {

static const char *TAG = "sn74hc595";

void SN74HC595Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SN74HC595...");

  if (this->have_oe_pin_) {  // disable output
    this->oe_pin_->pin_mode(OUTPUT);
    this->oe_pin_->digital_write(true);
  }

  // initialize output pins
  this->clock_pin_->pin_mode(OUTPUT);
  this->data_pin_->pin_mode(OUTPUT);
  this->latch_pin_->pin_mode(OUTPUT);
  this->clock_pin_->digital_write(LOW);
  this->data_pin_->digital_write(LOW);
  this->latch_pin_->digital_write(LOW);

  // send state to shift register
  this->write_gpio_();
}

void SN74HC595Component::dump_config() { ESP_LOGCONFIG(TAG, "SN74HC595:"); }

bool SN74HC595Component::digital_read_(uint8_t pin) { return this->output_bits_ >> pin; }

void SN74HC595Component::digital_write_(uint8_t pin, bool value) {
  uint32_t mask = 1UL << pin;
  this->output_bits_ &= ~mask;
  if (value)
    this->output_bits_ |= mask;
  this->write_gpio_();
}

bool SN74HC595Component::write_gpio_() {
  for (int i = this->sr_count_ - 1; i >= 0; i--) {
    uint8_t data = (uint8_t)(this->output_bits_ >> (8 * i) & 0xff);
    for (int j = 0; j < 8; j++) {
      this->data_pin_->digital_write(data & (1 << (7 - j)));
      this->clock_pin_->digital_write(true);
      this->clock_pin_->digital_write(false);
    }
  }

  // pulse latch to activate new values
  this->latch_pin_->digital_write(true);
  this->latch_pin_->digital_write(false);

  // enable output if configured
  if (this->have_oe_pin_) {
    this->oe_pin_->digital_write(false);
  }

  return true;
}

float SN74HC595Component::get_setup_priority() const { return setup_priority::IO; }

void SN74HC595GPIOPin::setup() {}

bool SN74HC595GPIOPin::digital_read() { return this->parent_->digital_read_(this->pin_) != this->inverted_; }

void SN74HC595GPIOPin::digital_write(bool value) {
  this->parent_->digital_write_(this->pin_, value != this->inverted_);
}

SN74HC595GPIOPin::SN74HC595GPIOPin(SN74HC595Component *parent, uint8_t pin, bool inverted)
    : GPIOPin(pin, OUTPUT, inverted), parent_(parent) {}

}  // namespace sn74hc595
}  // namespace esphome
