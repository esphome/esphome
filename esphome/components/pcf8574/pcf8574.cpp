#include "pcf8574.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pcf8574 {

static const char *TAG = "pcf8574";

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
    this->port_mask_ |= (1 << pin);
  } else {
    this->port_mask_ &= ~(1 << pin);
  }

  this->write_gpio_();
}
void PCF8574Component::pin_mode(uint8_t pin, uint8_t mode) {
  switch (mode) {
    case PCF8574_INPUT:
      this->ddr_mask_ &= ~(1 << pin);
      this->port_mask_ &= ~(1 << pin);
      break;
    case PCF8574_INPUT_PULLUP:
      this->ddr_mask_ &= ~(1 << pin);
      this->port_mask_ |= (1 << pin);
      break;
    case PCF8574_OUTPUT:
      this->ddr_mask_ |= (1 << pin);
      this->port_mask_ &= ~(1 << pin);
      break;
    default:
      break;
  }
}
bool PCF8574Component::read_gpio_() {
  if (this->is_failed())
    return false;

  if (this->pcf8575_) {
    if (!this->parent_->raw_receive_16(this->address_, &this->input_mask_, 1)) {
      this->status_set_warning();
      return false;
    }
  } else {
    uint8_t data;
    if (!this->parent_->raw_receive(this->address_, &data, 1)) {
      this->status_set_warning();
      return false;
    }
    this->input_mask_ = data;
  }

  this->status_clear_warning();
  return true;
}
bool PCF8574Component::write_gpio_() {
  if (this->is_failed())
    return false;

  uint16_t value = (this->input_mask_ & ~this->ddr_mask_) | this->port_mask_;

  this->parent_->raw_begin_transmission(this->address_);
  uint8_t data = value & 0xFF;
  this->parent_->raw_write(this->address_, &data, 1);

  if (this->pcf8575_) {
    data = (value >> 8) & 0xFF;
    this->parent_->raw_write(this->address_, &data, 1);
  }
  if (!this->parent_->raw_end_transmission(this->address_)) {
    this->status_set_warning();
    return false;
  }
  this->status_clear_warning();
  return true;
}
float PCF8574Component::get_setup_priority() const { return setup_priority::IO; }

void PCF8574GPIOPin::setup() { this->pin_mode(this->mode_); }
bool PCF8574GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void PCF8574GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
void PCF8574GPIOPin::pin_mode(uint8_t mode) { this->parent_->pin_mode(this->pin_, mode); }
PCF8574GPIOPin::PCF8574GPIOPin(PCF8574Component *parent, uint8_t pin, uint8_t mode, bool inverted)
    : GPIOPin(pin, mode, inverted), parent_(parent) {}

}  // namespace pcf8574
}  // namespace esphome
