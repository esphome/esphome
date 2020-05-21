#include "sn74hc595.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sn74hc595 {

static const char *TAG = "sn74hc595";

void SN74HC595::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SN74HC595...");
  this->spi_setup();
  this->enable();
  this->transfer_byte(0);  // All off
  this->disable();
}

float SN74HC595::get_setup_priority() const { return setup_priority::HARDWARE; }

void SN74HC595::digital_write(uint8_t pin, bool value) {
  if (pin > 7) {
    return;
  }

  bitWrite(this->olat_, pin, value);
  this->enable();
  this->transfer_byte(this->olat_);
  this->disable();
}

SN74HC595GpioPin::SN74HC595GpioPin(SN74HC595 *parent, uint8_t pin, bool inverted)
    : GPIOPin(pin, OUTPUT, inverted), parent_(parent) {}
void SN74HC595GpioPin::digital_write(bool value) {
  return this->parent_->digital_write(this->pin_, value != this->inverted_);
}

}  // namespace sn74hc595
}  // namespace esphome
