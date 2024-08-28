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
  if (!this->read_gpio_modes_()) {
    this->mark_failed();
    return;
  }
  if (!this->read_gpio_outputs_()) {
    this->mark_failed();
    return;
  }
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
  this->write_gpio_modes_();
}
void TCA9555Component::loop() { this->reset_pin_cache_(); }

bool TCA9555Component::read_gpio_outputs_() {
  if (this->is_failed())
    return false;
  uint8_t data[2];
  if (!this->read_bytes(TCA9555_OUTPUT_PORT_REGISTER_0, data, 2)) {
    this->status_set_warning("Failed to read output register");
    return false;
  }
  this->output_mask_ = (uint16_t(data[1]) << 8) | (uint16_t(data[0]) << 0);
  this->status_clear_warning();
  return true;
}

bool TCA9555Component::read_gpio_modes_() {
  if (this->is_failed())
    return false;
  uint8_t data[2];
  bool success = this->read_bytes(TCA9555_CONFIGURATION_PORT_0, data, 2);
  if (!success) {
    this->status_set_warning("Failed to read mode register");
    return false;
  }
  this->mode_mask_ = (uint16_t(data[1]) << 8) | (uint16_t(data[0]) << 0);

  this->status_clear_warning();
  return true;
}
bool TCA9555Component::digital_read_hw(uint8_t pin) {
  if (this->is_failed())
    return false;
  bool success;
  uint8_t data[2];
  success = this->read_bytes(TCA9555_INPUT_PORT_REGISTER_0, data, 2);
  this->input_mask_ = (uint16_t(data[1]) << 8) | (uint16_t(data[0]) << 0);

  if (!success) {
    this->status_set_warning("Failed to read input register");
    return false;
  }

  this->status_clear_warning();
  return true;
}

void TCA9555Component::digital_write_hw(uint8_t pin, bool value) {
  if (this->is_failed())
    return;

  if (value) {
    this->output_mask_ |= (1 << pin);
  } else {
    this->output_mask_ &= ~(1 << pin);
  }

  uint8_t data[2];
  data[0] = this->output_mask_;
  data[1] = this->output_mask_ >> 8;
  if (!this->write_bytes(TCA9555_OUTPUT_PORT_REGISTER_0, data, 2)) {
    this->status_set_warning("Failed to write output register");
    return;
  }

  this->status_clear_warning();
}

bool TCA9555Component::write_gpio_modes_() {
  if (this->is_failed())
    return false;
  uint8_t data[2];

  data[0] = this->mode_mask_;
  data[1] = this->mode_mask_ >> 8;
  if (!this->write_bytes(TCA9555_CONFIGURATION_PORT_0, data, 2)) {
    this->status_set_warning("Failed to write mode register");
    return false;
  }
  this->status_clear_warning();
  return true;
}

bool TCA9555Component::digital_read_cache(uint8_t pin) { return this->input_mask_ & (1 << pin); }

float TCA9555Component::get_setup_priority() const { return setup_priority::IO; }

void TCA9555GPIOPin::setup() { this->pin_mode(this->flags_); }
void TCA9555GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool TCA9555GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void TCA9555GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string TCA9555GPIOPin::dump_summary() const { return str_sprintf("%u via TCA9555", this->pin_); }

}  // namespace tca9555
}  // namespace esphome
