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
  bool log_input = false;
  static uint32_t last_log = 0;
  std::string pins;
  log_input = (millis() - last_log) > 1000;

  this->load_pin_->digital_write(false);
  delayMicroseconds(10);
  this->load_pin_->digital_write(true);
  delayMicroseconds(10);

  if (this->clock_inhibit_pin_ != nullptr)
    this->clock_inhibit_pin_->digital_write(false);

  for (int16_t i = (this->sr_count_ * 8) - 1; i >= 0; i--) {
    this->input_bits_[i] = this->data_pin_->digital_read();

    if (log_input)
      pins += this->input_bits_[i] ? "1" : "0";
    this->clock_pin_->digital_write(true);
    delayMicroseconds(10);
    this->clock_pin_->digital_write(false);
    delayMicroseconds(10);
  }

  if (log_input) {
    ESP_LOGD(TAG, "Read: %s", pins.c_str());
    last_log = millis();
  }

  if (this->clock_inhibit_pin_ != nullptr)
    this->clock_inhibit_pin_->digital_write(true);
}

float SN74HC165Component::get_setup_priority() const { return setup_priority::IO; }

bool SN74HC165GPIOPin::digital_read() { return this->parent_->digital_read_(this->pin_); }

std::string SN74HC165GPIOPin::dump_summary() const { return str_snprintf("%u via SN74HC165", 18, pin_); }

}  // namespace sn74hc165
}  // namespace esphome
