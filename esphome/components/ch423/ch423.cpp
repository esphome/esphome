#include "ch423.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ch423 {

static constexpr uint8_t CH423_REG_SYS = 0x24;    // Set system parameters (0x48 >> 1)
static constexpr uint8_t CH423_SYS_IO_OE = 0x01;  // IO output enable
static constexpr uint8_t CH423_SYS_OD_EN = 0x04;  // Open drain enable for OC pins
static constexpr uint8_t CH423_REG_IO = 0x30;     // Write/read IO7-IO0 (0x60 >> 1)
static constexpr uint8_t CH423_REG_IO_RD = 0x26;  // Read IO7-IO0 (0x4D >> 1, rounded down)
static constexpr uint8_t CH423_REG_OCL = 0x22;    // Write OC7-OC0 (0x44 >> 1)
static constexpr uint8_t CH423_REG_OCH = 0x23;    // Write OC15-OC8 (0x46 >> 1)

ESPHOME_LOG_TAG(TAG, "ch423");

void CH423Component::setup() {
  // set outputs before mode
  this->write_outputs_();
  // Set system parameters and check for errors
  bool success = this->write_reg_(CH423_REG_SYS, this->sys_params_);
  // Only read inputs if pins are configured for input (IO_OE not set)
  if (success && !(this->sys_params_ & CH423_SYS_IO_OE)) {
    success = this->read_inputs_();
  }
  if (!this->setup_check(success, LOG_STR("CH423 not detected")))
    return;

  ESP_LOGCONFIG(TAG, "Initialization complete. Warning: %d, Error: %d", this->status_has_warning(),
                this->status_has_error());
}

void CH423Component::loop() {
  // Clear all the previously read flags.
  this->pin_read_flags_ = 0x00;
}

void CH423Component::dump_config() { ESP_LOGCONFIG(TAG, "CH423:"); }

void CH423Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (pin < 8) {
    if (flags & gpio::FLAG_OUTPUT) {
      this->sys_params_ |= CH423_SYS_IO_OE;
    }
  } else if (pin >= 8 && pin < 24) {
    if (flags & gpio::FLAG_OPEN_DRAIN) {
      this->sys_params_ |= CH423_SYS_OD_EN;
    }
  }
}

bool CH423Component::digital_read(uint8_t pin) {
  if (this->pin_read_flags_ == 0 || this->pin_read_flags_ & (1 << pin)) {
    // Read values on first access or in case it's being read again in the same loop
    this->read_inputs_();
  }

  this->pin_read_flags_ |= (1 << pin);
  return (this->input_bits_ & (1 << pin)) != 0;
}

void CH423Component::digital_write(uint8_t pin, bool value) {
  assign_bits(this->output_bits_, 1 << pin, value);
  this->write_outputs_();
}

bool CH423Component::read_inputs_() {
  if (this->is_failed()) {
    return false;
  }
  // reading inputs requires IO_OE to be 0
  if (this->sys_params_ & CH423_SYS_IO_OE) {
    return false;
  }
  uint8_t result;
  if (!this->status_comm_result(this->read_reg_(CH423_REG_IO_RD, &result)))
    return false;
  this->input_bits_ = result;
  return true;
}

// Write a register. Can't use the standard write_byte() method because there is no single pre-configured i2c address.
// Both only set the warning; the caller clears it after its last step.
bool CH423Component::write_reg_(uint8_t reg, uint8_t value) {
  return this->status_comm_check(this->bus_->write_readv(reg, &value, 1, nullptr, 0) == i2c::ERROR_OK);
}

bool CH423Component::read_reg_(uint8_t reg, uint8_t *value) {
  return this->status_comm_check(this->bus_->write_readv(reg, nullptr, 0, value, 1) == i2c::ERROR_OK);
}

bool CH423Component::write_outputs_() {
  bool success = true;
  // Write IO7-IO0
  success &= this->write_reg_(CH423_REG_IO, static_cast<uint8_t>(this->output_bits_));
  // Write OC7-OC0
  success &= this->write_reg_(CH423_REG_OCL, static_cast<uint8_t>(this->output_bits_ >> 8));
  // Write OC15-OC8
  success &= this->write_reg_(CH423_REG_OCH, static_cast<uint8_t>(this->output_bits_ >> 16));
  return this->status_comm_result(success);
}

float CH423Component::get_setup_priority() const { return setup_priority::IO; }

}  // namespace esphome::ch423

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::ch423::CH423Component, "EXIO%u via CH423")
