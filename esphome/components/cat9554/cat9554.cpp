#include "cat9554.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cat9554 {

static const char *TAG = "cat9554";

static void ICACHE_RAM_ATTR HOT gpio_intr(bool *need_update_gpio) { *need_update_gpio = true; }

void CAT9554Component::setup() {
  if (!this->read_gpio_()) {
    ESP_LOGE(TAG, "CAT9554 not available under 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  this->irq_pin_->setup();
  this->isr_ = this->irq_pin_->to_isr();
  this->irq_pin_->attach_interrupt(gpio_intr, &this->update_gpio_, FALLING);
  this->read_gpio_();
  this->read_config_();
  this->update_gpio_ = false;
}
void CAT9554Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CAT9554:");
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with CAT9554 failed!");
  }
}
bool CAT9554Component::digital_read(uint8_t pin) {
  if (this->update_gpio_) {
    this->read_gpio_();
    this->update_gpio_ = false;
  }
  return this->input_mask_ & (1 << pin);
}
void CAT9554Component::digital_write(uint8_t pin, bool value) {
  if (value) {
    this->output_mask_ |= (1 << pin);
  } else {
    this->output_mask_ &= ~(1 << pin);
  }

  this->write_gpio_();
}
void CAT9554Component::pin_mode(uint8_t pin, uint8_t mode) {
  switch (mode) {
    case CAT9554_INPUT:
      // Clear mode mask bit
      this->config_mask_ |= (1 << pin);
      break;
    case CAT9554_OUTPUT:
      // Set mode mask bit
      this->config_mask_ &= ~(1 << pin);
      break;
    default:
      break;
  }
  this->config_gpio_();
}
bool CAT9554Component::read_gpio_() {
  if (this->is_failed())
    return false;

  bool success;
  uint8_t data;
  success = this->read_byte(INPUT_REG, &data, 1);
  if (!success) {
    this->status_set_warning();
    return false;
  }
  this->input_mask_ = data;

  this->status_clear_warning();
  return true;
}
bool CAT9554Component::write_gpio_() {
  if (this->is_failed())
    return false;

  if (!this->write_byte(OUTPUT_REG, this->output_mask_)) {
    this->status_set_warning();
    return false;
  }

  this->status_clear_warning();
  return true;
}
bool CAT9554Component::config_gpio_() {
  if (this->is_failed())
    return false;

  if (!this->write_byte(INPUT_REG, this->config_mask_)) {
    this->status_set_warning();
    return false;
  }
  if (!this->write_byte(CONFIG_REG, this->config_mask_)) {
    this->status_set_warning();
    return false;
  }
  if (!this->write_byte(INPUT_REG, 0x00)) {
    this->status_set_warning();
    return false;
  }

  this->status_clear_warning();
  return true;
}
bool CAT9554Component::read_config_() {
  uint8_t data;

  if (this->is_failed())
    return false;

  if (!this->read_byte(CONFIG_REG, &data, 1)) {
    this->status_set_warning();
    return false;
  }
  this->config_mask_ = data;

  this->status_clear_warning();
  return true;
}
float CAT9554Component::get_setup_priority() const { return setup_priority::IO; }

void CAT9554GPIOPin::setup() { this->pin_mode(this->mode_); }
bool CAT9554GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void CAT9554GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
void CAT9554GPIOPin::pin_mode(uint8_t mode) { this->parent_->pin_mode(this->pin_, mode); }
CAT9554GPIOPin::CAT9554GPIOPin(CAT9554Component *parent, uint8_t pin, uint8_t mode, bool inverted)
    : GPIOPin(pin, mode, inverted), parent_(parent) {}

}  // namespace cat9554
}  // namespace esphome
