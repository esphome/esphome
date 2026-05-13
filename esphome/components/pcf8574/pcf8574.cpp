#include "pcf8574.h"
#include "esphome/core/log.h"

namespace esphome::pcf8574 {

static const char *const TAG = "pcf8574";

void PCF8574Component::setup() {
  if (!this->read_gpio_()) {
    ESP_LOGE(TAG, "PCF8574 not available under 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  this->write_gpio_();
  this->read_gpio_();

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->attach_interrupt(&PCF8574Component::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
    // Don't invalidate cache on read — only invalidate when interrupt fires
    this->set_invalidate_on_read_(false);
  }
  // Disable loop until an input pin is configured via pin_mode()
  // For interrupt-driven mode, loop is re-enabled by the ISR
  // For polling mode, loop is re-enabled when pin_mode() registers an input pin
  this->disable_loop();
}
void IRAM_ATTR PCF8574Component::gpio_intr(PCF8574Component *arg) { arg->enable_loop_soon_any_context(); }
void PCF8574Component::loop() {
  // Invalidate the cache so the next digital_read() triggers a fresh I2C read
  this->reset_pin_cache_();
  // Only disable the loop once INT has actually gone HIGH. Input transitions that straddle the
  // I2C read leave INT asserted without re-firing a falling edge, which would strand us with
  // stale state forever; keep looping until the line is released so we self-heal.
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read()) {
    this->disable_loop();
  }
}
void PCF8574Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PCF8574:\n"
                "  Is PCF8575: %s",
                YESNO(this->pcf8575_));
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}
bool PCF8574Component::digital_read_hw(uint8_t pin) {
  // Read all pins from hardware into input_mask_
  return this->read_gpio_();  // Return true if I2C read succeeded, false on error
}

bool PCF8574Component::digital_read_cache(uint8_t pin) { return this->input_mask_ & (1 << pin); }

void PCF8574Component::digital_write_hw(uint8_t pin, bool value) {
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
    // Enable polling loop for input pins (not needed for interrupt-driven mode
    // where the ISR handles re-enabling loop)
    if (this->interrupt_pin_ == nullptr) {
      this->enable_loop();
    }
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
size_t PCF8574GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via PCF8574", this->pin_);
}

}  // namespace esphome::pcf8574
