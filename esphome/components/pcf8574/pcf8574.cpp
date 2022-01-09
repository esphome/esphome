#include "pcf8574.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pcf8574 {

static const char *const TAG = "pcf8574";

void PCF8574Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PCF8574...");
  if (!this->read_gpio_()) {
    ESP_LOGE(TAG, "PCF8574 not available under 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  this->write_gpio_();
  this->read_gpio_();
}
void PCF8574Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PCF8574:");
  LOG_I2C_DEVICE(this)
  ESP_LOGCONFIG(TAG, "  Is PCF8575: %s", YESNO(this->pcf8575_));
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with PCF8574 failed!");
  }
}
bool PCF8574Component::digital_read(uint8_t pin) {
  this->read_gpio_();
  return this->input_mask_ & (1 << pin);
}
void PCF8574Component::digital_write(uint8_t pin, bool value) {
  if (value) {
    this->output_mask_ |= (1 << pin);
  } else {
    this->output_mask_ &= ~(1 << pin);
  }

  this->write_gpio_();
}
void PCF8574Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (flags == gpio::FLAG_INPUT) {
    // Clear mode mask bit
    this->mode_mask_ &= ~(1 << pin);
    // Write GPIO to enable input mode
    this->write_gpio_();
  } else if (flags == gpio::FLAG_OUTPUT) {
    // Set mode mask bit
    this->mode_mask_ |= 1 << pin;
  }
}
bool PCF8574Component::read_gpio_() {
  if (this->is_failed())
    return false;
  bool success;
  uint8_t data[2];
  if (this->pcf8575_) {
    success = this->read_bytes_raw(data, 2);
    this->input_mask_ = (uint16_t(data[1]) << 8) | (uint16_t(data[0]) << 0);
  } else {
    success = this->read_bytes_raw(data, 1);
    this->input_mask_ = data[0];
  }

  if (!success) {
    this->status_set_warning();
    return false;
  }
  this->status_clear_warning();
  return true;
}
bool PCF8574Component::write_gpio_() {
  if (this->is_failed())
    return false;

  uint16_t value = 0;
  // Pins in OUTPUT mode and where pin is HIGH.
  value |= this->mode_mask_ & this->output_mask_;
  // Pins in INPUT mode must also be set here
  value |= ~this->mode_mask_;

  uint8_t data[2];
  data[0] = value;
  data[1] = value >> 8;
  if (this->write(data, this->pcf8575_ ? 2 : 1) != i2c::ERROR_OK) {
    this->status_set_warning();
    return false;
  }

  this->status_clear_warning();
  return true;
}
float PCF8574Component::get_setup_priority() const { return setup_priority::IO; }

void PCF8574GPIOPin::setup() { pin_mode(flags_); }
void PCF8574GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool PCF8574GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void PCF8574GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string PCF8574GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via PCF8574", pin_);
  return buffer;
}

}  // namespace pcf8574
}  // namespace esphome
