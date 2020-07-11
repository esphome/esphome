#include "sn74hc595.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sn74hc595 {

static const char *TAG = "sn74hc595";

void SN74HC595Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SN74HC595...");

  if (this->have_oe_pin_) {  // disable output
    pinMode(this->oe_pin_->get_pin(), OUTPUT);
    digitalWrite(this->oe_pin_->get_pin(), HIGH);
  }

  // initialize output pins
  pinMode(this->clock_pin_->get_pin(), OUTPUT);
  pinMode(this->data_pin_->get_pin(), OUTPUT);
  pinMode(this->latch_pin_->get_pin(), OUTPUT);
  digitalWrite(this->clock_pin_->get_pin(), LOW);
  digitalWrite(this->data_pin_->get_pin(), LOW);
  digitalWrite(this->latch_pin_->get_pin(), LOW);

  // send state to shift register
  this->write_gpio_();
}

void SN74HC595Component::dump_config() { ESP_LOGCONFIG(TAG, "SN74HC595:"); }

bool SN74HC595Component::digital_read_(uint8_t pin) { return bitRead(this->output_bits_, pin); }

void SN74HC595Component::digital_write_(uint8_t pin, bool value) {
  bitWrite(this->output_bits_, pin, value);
  this->write_gpio_();
}

bool SN74HC595Component::write_gpio_() {
  for (int i = this->sr_count_ - 1; i >= 0; i--) {
    uint8_t data = (uint8_t)(this->output_bits_ >> (8 * i) & 0xff);
    shiftOut(this->data_pin_->get_pin(), this->clock_pin_->get_pin(), MSBFIRST, data);
  }

  // pulse latch to activate new values
  digitalWrite(this->latch_pin_->get_pin(), HIGH);
  digitalWrite(this->latch_pin_->get_pin(), LOW);

  // enable output if configured
  if (this->have_oe_pin_) {
    digitalWrite(this->oe_pin_->get_pin(), LOW);
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
