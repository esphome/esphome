#include "sn74hc165.h"
#include "esphome/core/log.h"

namespace esphome::sn74hc165 {

ESPHOME_LOG_TAG(TAG, "sn74hc165");

void SN74HC165Component::setup() {
  // initialize pins
  setup_output_pin(this->clock_pin_, false);
  this->data_pin_->setup();
  setup_output_pin(this->load_pin_, false);

  setup_output_pin(this->clock_inhibit_pin_, true);

  // read state from shift register
  this->read_gpio_();
}

void SN74HC165Component::loop() { this->read_gpio_(); }

void SN74HC165Component::dump_config() { ESP_LOGCONFIG(TAG, "SN74HC165:"); }

bool SN74HC165Component::digital_read_(uint16_t pin) {
  if (pin >= this->input_bytes_.size() * 8) {
    ESP_LOGE(TAG, "Pin %u is out of range! Maximum pin number with %zu chips in series is %zu", pin,
             this->input_bytes_.size(), (this->input_bytes_.size() * 8) - 1);
    return false;
  }
  return (this->input_bytes_[pin / 8] >> (pin % 8)) & 1;
}

void SN74HC165Component::read_gpio_() {
  pulse_pin(this->load_pin_, false, 10);
  delayMicroseconds(10);

  if (this->clock_inhibit_pin_ != nullptr)
    this->clock_inhibit_pin_->digital_write(false);

  for (uint8_t &input_byte : this->input_bytes_) {
    input_byte = 0;
    for (uint8_t j = 0; j < 8; j++) {
      input_byte = (input_byte << 1) | this->data_pin_->digital_read();

      pulse_pin(this->clock_pin_, true, 10);
      delayMicroseconds(10);
    }
  }

  if (this->clock_inhibit_pin_ != nullptr)
    this->clock_inhibit_pin_->digital_write(true);
}

float SN74HC165Component::get_setup_priority() const { return setup_priority::IO; }

bool SN74HC165GPIOPin::digital_read() { return this->parent_->digital_read_(this->pin_) != this->inverted_; }

size_t SN74HC165GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via SN74HC165", this->pin_);
}

}  // namespace esphome::sn74hc165
