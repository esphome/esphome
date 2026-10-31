#include "pcf8574.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::pcf8574 {

ESPHOME_LOG_TAG(TAG, "pcf8574");

void PCF8574Component::setup() {
  if (!this->read_gpio_()) {
    ESP_LOGE(TAG, "PCF8574 not available under 0x%02X", this->address_);
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  if (this->reset_) {
    this->write_gpio_();
    this->read_gpio_();
  } else {
    this->output_mask_ = this->input_[0];
    this->mode_mask_ = ~this->configured_input_pins_;
  }

  this->setup_interrupt_pin_();
}
void PCF8574Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PCF8574:\n"
                "  Is PCF8575: %s\n"
                "  Reset: %s",
                YESNO(this->pcf8575_), YESNO(this->reset_));
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_I2C_DEVICE(this)
}
bool PCF8574Component::digital_read_hw(uint8_t pin) {
  // Read all pins from hardware into input_[0]
  return this->read_gpio_();  // Return true if I2C read succeeded, false on error
}

void PCF8574Component::digital_write_hw(uint8_t pin, bool value) {
  assign_bits(this->output_mask_, 1 << pin, value);
  this->write_gpio_();
}
void PCF8574Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (flags == gpio::FLAG_INPUT) {
    // Clear mode mask bit
    this->mode_mask_ &= ~(1 << pin);
    // Write GPIO to enable input mode
    this->write_gpio_();
    this->enable_polling_loop_();
  } else if (flags == gpio::FLAG_OUTPUT) {
    // Set mode mask bit
    this->mode_mask_ |= 1 << pin;
  }
}
bool PCF8574Component::read_gpio_() {
  if (this->is_failed())
    return false;
  uint8_t data[2]{};  // Keep the bytes zero for the PCF8574's one-byte read.
  if (!this->status_comm_result(this->read_bytes_raw(data, this->pcf8575_ ? 2 : 1)))
    return false;
  this->input_[0] = encode_value_le<uint16_t>(data);
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
  return this->status_comm_result(this->write(data, this->pcf8575_ ? 2 : 1) == i2c::ERROR_OK);
}

}  // namespace esphome::pcf8574

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::pcf8574::PCF8574Component, "%u via PCF8574")
