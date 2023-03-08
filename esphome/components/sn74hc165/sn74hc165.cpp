#include "sn74hc165.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sn74hc165 {

static const char *const TAG = "sn74hc165";

void SN74HC165Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SN74HC165...");

  // initialize pins
  this->clock_pin_->setup();
  this->data_pin_->setup();
  this->load_pin_->setup();
  this->clock_pin_->digital_write(false);
  this->load_pin_->digital_write(false);

  if (this->clock_inhibit_pin_ != nullptr) {
    this->clock_inhibit_pin_->setup();
    this->clock_inhibit_pin_->digital_write(true);
  }

  // read state from shift register
  this->read_gpio_();
}

void SN74HC165Component::loop() { this->read_gpio_(); }

void SN74HC165Component::dump_config() { ESP_LOGCONFIG(TAG, "SN74HC165:"); }

bool SN74HC165Component::digital_read_(uint16_t pin) {
  if (pin >= this->sr_count_ * 8) {
    ESP_LOGE(TAG, "Pin %u is out of range! Maximum pin number with %u chips in series is %u", pin, this->sr_count_,
             (this->sr_count_ * 8) - 1);
    return false;
  }
  return this->input_bits_[pin];
}

void SN74HC165Component::read_gpio_() {
  this->load_pin_->digital_write(false);
  delayMicroseconds(10);
  this->load_pin_->digital_write(true);
  delayMicroseconds(10);

  if (this->clock_inhibit_pin_ != nullptr)
    this->clock_inhibit_pin_->digital_write(false);

  for (uint8_t i = 0; i < this->sr_count_; i++) {
    for (uint8_t j = 0; j < 8; j++) {
      this->input_bits_[(i * 8) + (7 - j)] = this->data_pin_->digital_read();

      this->clock_pin_->digital_write(true);
      delayMicroseconds(10);
      this->clock_pin_->digital_write(false);
      delayMicroseconds(10);
    }
  }

  if (this->clock_inhibit_pin_ != nullptr)
    this->clock_inhibit_pin_->digital_write(true);
}

float SN74HC165Component::get_setup_priority() const { return setup_priority::IO; }

bool SN74HC165GPIOPin::digital_read() { return this->parent_->digital_read_(this->pin_) != this->inverted_; }

std::string SN74HC165GPIOPin::dump_summary() const { return str_snprintf("%u via SN74HC165", 18, pin_); }

}  // namespace sn74hc165
}  // namespace esphome
