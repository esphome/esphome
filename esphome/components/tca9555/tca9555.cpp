#include "tca9555.h"
#include "esphome/core/log.h"

static const uint8_t TCA9555_INPUT_PORT_REGISTER_0 = 0x00;
static const uint8_t TCA9555_INPUT_PORT_REGISTER_1 = 0x01;
static const uint8_t TCA9555_OUTPUT_PORT_REGISTER_0 = 0x02;
static const uint8_t TCA9555_OUTPUT_PORT_REGISTER_1 = 0x03;
static const uint8_t TCA9555_POLARITY_REGISTER_0 = 0x04;
static const uint8_t TCA9555_POLARITY_REGISTER_1 = 0x05;
static const uint8_t TCA9555_CONFIGURATION_PORT_0 = 0x06;
static const uint8_t TCA9555_CONFIGURATION_PORT_1 = 0x07;

namespace esphome {
namespace tca9555 {

static const char *const TAG = "tca9555";

void TCA9555Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TCA9555...");
  if (!this->read_gpio_current_config_()) {
    ESP_LOGE(TAG, "TCA9555 not available under 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  this->write_gpio_hw();
}
void TCA9555Component::dump_config() {
  ESP_LOGCONFIG(TAG, "TCA9555:");
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with TCA9555 failed!");
  }
}
void TCA9555Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (flags == gpio::FLAG_INPUT) {
    // Set mode mask bit
    this->mode_mask_ |= 1 << pin;
  } else if (flags == gpio::FLAG_OUTPUT) {
    // Clear mode mask bit
    this->mode_mask_ &= ~(1 << pin);
  }
  // Write GPIO to enable input mode
  this->write_gpio_hw();
}
bool TCA9555Component::digital_read(uint8_t pin) { return cached_gpio_.digital_read(pin); }
void TCA9555Component::digital_write(uint8_t pin, bool value) { cached_gpio_.digital_write(pin, value); }
void TCA9555Component::loop() { cached_gpio_.loop(); }
bool TCA9555Component::read_gpio_current_config_() {
  if (this->is_failed())
    return false;
  bool success;
  uint8_t data[2];
  success = this->read_bytes(TCA9555_OUTPUT_PORT_REGISTER_0, data, 2);
  if (!success) {
    ESP_LOGE(TAG, "Failed to read Output register after reset!");
    this->status_set_warning();
    return false;
  }
  this->output_mask_ = (uint16_t(data[1]) << 8) | (uint16_t(data[0]) << 0);

  success = this->read_bytes(TCA9555_CONFIGURATION_PORT_0, data, 2);
  if (!success) {
    ESP_LOGE(TAG, "Failed to read Mode register after reset!");
    this->status_set_warning();
    return false;
  }
  this->mode_mask_ = (uint16_t(data[1]) << 8) | (uint16_t(data[0]) << 0);

  this->status_clear_warning();
  return true;
}
bool TCA9555Component::read_gpio_hw() {
  if (this->is_failed())
    return false;
  bool success;
  uint8_t data[2];
  success = this->read_bytes(TCA9555_INPUT_PORT_REGISTER_0, data, 2);
  this->input_mask_ = (uint16_t(data[1]) << 8) | (uint16_t(data[0]) << 0);

  if (!success) {
    ESP_LOGE(TAG, "Failed to read Input register!");
    this->status_set_warning();
    return false;
  }

  this->status_clear_warning();
  return true;
}
bool TCA9555Component::write_gpio_hw() {
  if (this->is_failed())
    return false;

  uint8_t data[2];

  data[0] = this->mode_mask_;
  data[1] = this->mode_mask_ >> 8;
  if (!this->write_bytes(TCA9555_CONFIGURATION_PORT_0, data, 2)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Mode mask set failed!");
    return false;
  }

  data[0] = this->output_mask_;
  data[1] = this->output_mask_ >> 8;
  if (!this->write_bytes(TCA9555_OUTPUT_PORT_REGISTER_0, data, 2)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "Output register set failed!");
    return false;
  }

  this->status_clear_warning();
  return true;
}

bool TCA9555Component::read_gpio_from_cache(uint8_t pin) { return this->input_mask_ & (1 << pin); }

bool TCA9555Component::write_gpio_from_cache(uint8_t pin, bool value) {
  if (value) {
    this->output_mask_ |= (1 << pin);
  } else {
    this->output_mask_ &= ~(1 << pin);
  }
  return true;
}

float TCA9555Component::get_setup_priority() const { return setup_priority::IO; }

void TCA9555GPIOPin::setup() { this->pin_mode(this->flags_); }
void TCA9555GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool TCA9555GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void TCA9555GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string TCA9555GPIOPin::dump_summary() const { return str_sprintf("%u via TCA9555", this->pin_); }

}  // namespace tca9555
}  // namespace esphome
