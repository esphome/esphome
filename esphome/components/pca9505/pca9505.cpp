#include "pca9505.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::pca9505 {

// Each register block holds one byte per port. Add the port number to reach a single
// port, or set the auto-increment bit to transfer the whole block in one go.
static constexpr uint8_t INPUT_REG = 0x00;
static constexpr uint8_t OUTPUT_REG = 0x08;
static constexpr uint8_t INVERT_REG = 0x10;
static constexpr uint8_t CONFIG_REG = 0x18;
static constexpr uint8_t MASK_REG = 0x20;
static constexpr uint8_t AUTO_INCREMENT = 0x80;

ESPHOME_LOG_TAG(TAG, "pca9505");

void PCA9505Component::setup() {
  // Test to see if device exists
  if (!this->read_bank_(0)) {
    ESP_LOGE(TAG, "PCA9505 not detected at 0x%02X", this->address_);
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }

  // No polarity inversion
  this->write_all_banks_(INVERT_REG, 0x00);
  // All outputs low, before anything is switched to being an output
  this->write_all_banks_(OUTPUT_REG, 0x00);
  // All inputs at initialization. The part sees a 1 as an input, so this is the
  // inverse of config_, which is all zeroes here.
  this->write_all_banks_(CONFIG_REG, 0xFF);

  // Mask every interrupt. This must be written, not assumed: the chip keeps its
  // registers through an MCU reboot, so a previous run may have left pins unmasked.
  for (uint8_t &bank : this->mask_) {
    bank = 0xFF;
  }
  this->write_all_banks_(MASK_REG, 0xFF);

  this->setup_interrupt_pin_();
}

void PCA9505Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PCA9505:");
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_I2C_DEVICE(this)
}

bool PCA9505Component::digital_read_hw(uint8_t pin) { return this->read_bank_(pin / 8); }

void PCA9505Component::digital_write_hw(uint8_t pin, bool value) {
  const uint8_t bank = pin / 8;
  const uint8_t mask = 1 << (pin % 8);
  assign_bits(this->output_[bank], mask, value);
  this->write_bank_(OUTPUT_REG, bank, this->output_[bank]);
}

void PCA9505Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  const uint8_t bank = pin / 8;
  const uint8_t mask = 1 << (pin % 8);
  if (flags & gpio::FLAG_INPUT) {
    this->config_[bank] &= ~mask;
    // An input may raise INT.
    this->mask_[bank] &= ~mask;
    this->enable_polling_loop_();
  } else if (flags & gpio::FLAG_OUTPUT) {
    this->config_[bank] |= mask;
    // An output must never raise INT.
    this->mask_[bank] |= mask;
  }
  // Invert as the part sees a 1 as an input
  this->write_bank_(CONFIG_REG, bank, ~this->config_[bank]);
  // Without an interrupt pin INT is unconnected, so the masks written at setup can stay
  if (this->interrupt_pin_ != nullptr) {
    this->write_bank_(MASK_REG, bank, this->mask_[bank]);
  }
}

bool PCA9505Component::read_bank_(uint8_t bank) {
  if (this->is_failed())
    return false;

  return this->status_comm_result(this->read_register(INPUT_REG + bank, &this->input_[bank], 1) == i2c::ERROR_OK,
                                  LOG_STR("Failed to read input register"));
}

bool PCA9505Component::write_bank_(uint8_t reg, uint8_t bank, uint8_t value) {
  if (this->is_failed())
    return false;

  return this->status_comm_result(this->write_register(reg + bank, &value, 1) == i2c::ERROR_OK,
                                  LOG_STR("Failed to write register"));
}

bool PCA9505Component::write_all_banks_(uint8_t reg, uint8_t value) {
  if (this->is_failed())
    return false;

  uint8_t values[BANK_COUNT];
  for (uint8_t &bank : values) {
    bank = value;
  }
  return this->status_comm_result(this->write_register(reg | AUTO_INCREMENT, values, sizeof(values)) == i2c::ERROR_OK,
                                  LOG_STR("Failed to write register"));
}

}  // namespace esphome::pca9505

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::pca9505::PCA9505Component, "%u via PCA9505")
