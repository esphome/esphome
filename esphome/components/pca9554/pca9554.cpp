#include "pca9554.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pca9554 {

const uint8_t INPUT_REG = 0;
const uint8_t OUTPUT_REG = 1;
const uint8_t INVERT_REG = 2;
const uint8_t CONFIG_REG = 3;

static const char *const TAG = "pca9554";

void PCA9554Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PCA9554/PCA9554A...");
  // Test to see if device exists
  if (!this->read_inputs_()) {
    ESP_LOGE(TAG, "PCA9554 not available under 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // No polarity inversion
  this->write_register_(INVERT_REG, 0);
  // All inputs at initialization
  this->config_mask_ = 0;
  // Invert mask as the part sees a 1 as an input
  this->write_register_(CONFIG_REG, ~this->config_mask_);
  // All ouputs low
  this->output_mask_ = 0;
  this->write_register_(OUTPUT_REG, this->output_mask_);
  // Read the inputs
  this->read_inputs_();
  ESP_LOGD(TAG, "Initialization complete. Warning: %d, Error: %d", this->status_has_warning(),
           this->status_has_error());
}
void PCA9554Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PCA9554:");
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with PCA9554 failed!");
  }
}

bool PCA9554Component::digital_read(uint8_t pin) {
  this->read_inputs_();
  return this->input_mask_ & (1 << pin);
}

void PCA9554Component::digital_write(uint8_t pin, bool value) {
  if (value) {
    this->output_mask_ |= (1 << pin);
  } else {
    this->output_mask_ &= ~(1 << pin);
  }
  this->write_register_(OUTPUT_REG, this->output_mask_);
}

void PCA9554Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (flags == gpio::FLAG_INPUT) {
    // Clear mode mask bit
    this->config_mask_ &= ~(1 << pin);
  } else if (flags == gpio::FLAG_OUTPUT) {
    // Set mode mask bit
    this->config_mask_ |= 1 << pin;
  }
  this->write_register_(CONFIG_REG, ~this->config_mask_);
}

bool PCA9554Component::read_inputs_() {
  uint8_t inputs;

  if (this->is_failed()) {
    ESP_LOGD(TAG, "Device marked failed");
    return false;
  }

  if ((this->last_error_ = this->read_register(INPUT_REG, &inputs, 1, true)) != esphome::i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "read_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }
  this->status_clear_warning();
  this->input_mask_ = inputs;
  return true;
}

bool PCA9554Component::write_register_(uint8_t reg, uint8_t value) {
  if ((this->last_error_ = this->write_register(reg, &value, 1, true)) != esphome::i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }

  this->status_clear_warning();
  return true;
}

float PCA9554Component::get_setup_priority() const { return setup_priority::IO; }

void PCA9554GPIOPin::setup() { pin_mode(flags_); }
void PCA9554GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool PCA9554GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void PCA9554GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string PCA9554GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via PCA9554", pin_);
  return buffer;
}

}  // namespace pca9554
}  // namespace esphome
