#include "pca9505.h"
#include "esphome/core/log.h"

namespace esphome::pca9505 {

// Each register block holds one byte per port. Add the port number to reach a single
// port, or set the auto-increment bit to transfer the whole block in one go.
const uint8_t INPUT_REG = 0x00;
const uint8_t OUTPUT_REG = 0x08;
const uint8_t INVERT_REG = 0x10;
const uint8_t CONFIG_REG = 0x18;
const uint8_t AUTO_INCREMENT = 0x80;

ESPHOME_LOG_TAG(TAG, "pca9505");

void PCA9505Component::setup() {
  // Test to see if device exists
  if (!this->read_bank_(0)) {
    ESP_LOGE(TAG, "PCA9505 not detected at 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // No polarity inversion
  this->write_all_banks_(INVERT_REG, 0x00);
  // All outputs low, before anything is switched to being an output
  this->write_all_banks_(OUTPUT_REG, 0x00);
  // All inputs at initialization. The part sees a 1 as an input, so this is the
  // inverse of config_, which is all zeroes here.
  this->write_all_banks_(CONFIG_REG, 0xFF);

  // Nothing to poll until pin_mode() registers an input pin.
  this->disable_loop();
}

void PCA9505Component::loop() {
  // Invalidate the cache so the next digital_read() triggers a fresh I2C read
  this->reset_pin_cache_();
}

void PCA9505Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PCA9505:");
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

bool PCA9505Component::digital_read_hw(uint8_t pin) { return this->read_bank_(pin / 8); }

bool PCA9505Component::digital_read_cache(uint8_t pin) { return this->input_[pin / 8] & (1 << (pin % 8)); }

void PCA9505Component::digital_write_hw(uint8_t pin, bool value) {
  const uint8_t bank = pin / 8;
  const uint8_t mask = 1 << (pin % 8);
  if (value) {
    this->output_[bank] |= mask;
  } else {
    this->output_[bank] &= ~mask;
  }
  this->write_bank_(OUTPUT_REG, bank, this->output_[bank]);
}

void PCA9505Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  const uint8_t bank = pin / 8;
  const uint8_t mask = 1 << (pin % 8);
  if (flags & gpio::FLAG_INPUT) {
    this->config_[bank] &= ~mask;
    // An input pin has to be polled for changes
    this->enable_loop();
  } else if (flags & gpio::FLAG_OUTPUT) {
    this->config_[bank] |= mask;
  }
  // Invert as the part sees a 1 as an input
  this->write_bank_(CONFIG_REG, bank, ~this->config_[bank]);
}

bool PCA9505Component::read_bank_(uint8_t bank) {
  if (this->is_failed())
    return false;

  if (this->read_register(INPUT_REG + bank, &this->input_[bank], 1) != i2c::ERROR_OK) {
    this->status_set_warning("read register failed");
    return false;
  }
  this->status_clear_warning();
  return true;
}

bool PCA9505Component::write_bank_(uint8_t reg, uint8_t bank, uint8_t value) {
  if (this->is_failed())
    return false;

  if (this->write_register(reg + bank, &value, 1) != i2c::ERROR_OK) {
    this->status_set_warning("write register failed");
    return false;
  }
  this->status_clear_warning();
  return true;
}

bool PCA9505Component::write_all_banks_(uint8_t reg, uint8_t value) {
  if (this->is_failed())
    return false;

  uint8_t values[BANK_COUNT];
  for (uint8_t &bank : values) {
    bank = value;
  }
  if (this->write_register(reg | AUTO_INCREMENT, values, sizeof(values)) != i2c::ERROR_OK) {
    this->status_set_warning("write register failed");
    return false;
  }
  this->status_clear_warning();
  return true;
}

float PCA9505Component::get_setup_priority() const { return setup_priority::IO; }

void PCA9505GPIOPin::setup() { pin_mode(flags_); }

void PCA9505GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }

bool PCA9505GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }

void PCA9505GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }

size_t PCA9505GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via PCA9505", this->pin_);
}

}  // namespace esphome::pca9505
