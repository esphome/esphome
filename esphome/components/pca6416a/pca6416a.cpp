#include "pca6416a.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pca6416a {

enum PCA6416AGPIORegisters {
  // 0 side
  PCA6416A_INPUT0 = 0x00,
  PCA6416A_OUTPUT0 = 0x02,
  PCA6416A_INVERT0 = 0x04,
  PCA6416A_CONFIG0 = 0x06,
  PCAL6416A_PULL_EN0 = 0x46,
  PCAL6416A_PULL_DIR0 = 0x48,
  // 1 side
  PCA6416A_INPUT1 = 0x01,
  PCA6416A_OUTPUT1 = 0x03,
  PCA6416A_INVERT1 = 0x05,
  PCA6416A_CONFIG1 = 0x07,
  PCAL6416A_PULL_EN1 = 0x47,
  PCAL6416A_PULL_DIR1 = 0x49,
};

static const char *const TAG = "pca6416a";

void PCA6416AComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PCA6416A...");
  // Test to see if device exists
  uint8_t value;
  if (!this->read_register_(PCA6416A_INPUT0, &value)) {
    ESP_LOGE(TAG, "PCA6416A not available under 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // Test to see if the device supports pull-up resistors
  if (this->read_register(PCAL6416A_PULL_EN0, &value, 1, true) == i2c::ERROR_OK) {
    this->has_pullup_ = true;
  }

  // No polarity inversion
  this->write_register_(PCA6416A_INVERT0, 0);
  this->write_register_(PCA6416A_INVERT1, 0);
  // Set all pins to input
  this->write_register_(PCA6416A_CONFIG0, 0xff);
  this->write_register_(PCA6416A_CONFIG1, 0xff);
  // Read current output register state
  this->read_register_(PCA6416A_OUTPUT0, &this->output_0_);
  this->read_register_(PCA6416A_OUTPUT1, &this->output_1_);

  ESP_LOGD(TAG, "Initialization complete. Warning: %d, Error: %d", this->status_has_warning(),
           this->status_has_error());
}

void PCA6416AComponent::dump_config() {
  if (this->has_pullup_) {
    ESP_LOGCONFIG(TAG, "PCAL6416A:");
  } else {
    ESP_LOGCONFIG(TAG, "PCA6416A:");
  }
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with PCA6416A failed!");
  }
}

bool PCA6416AComponent::digital_read(uint8_t pin) {
  uint8_t bit = pin % 8;
  uint8_t reg_addr = pin < 8 ? PCA6416A_INPUT0 : PCA6416A_INPUT1;
  uint8_t value = 0;
  this->read_register_(reg_addr, &value);
  return value & (1 << bit);
}

void PCA6416AComponent::digital_write(uint8_t pin, bool value) {
  uint8_t reg_addr = pin < 8 ? PCA6416A_OUTPUT0 : PCA6416A_OUTPUT1;
  this->update_register_(pin, value, reg_addr);
}

void PCA6416AComponent::pin_mode(uint8_t pin, gpio::Flags flags) {
  uint8_t io_dir = pin < 8 ? PCA6416A_CONFIG0 : PCA6416A_CONFIG1;
  uint8_t pull_en = pin < 8 ? PCAL6416A_PULL_EN0 : PCAL6416A_PULL_EN1;
  uint8_t pull_dir = pin < 8 ? PCAL6416A_PULL_DIR0 : PCAL6416A_PULL_DIR1;
  if (flags == gpio::FLAG_INPUT) {
    this->update_register_(pin, true, io_dir);
    if (has_pullup_) {
      this->update_register_(pin, true, pull_dir);
      this->update_register_(pin, false, pull_en);
    }
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    this->update_register_(pin, true, io_dir);
    if (has_pullup_) {
      this->update_register_(pin, true, pull_dir);
      this->update_register_(pin, true, pull_en);
    } else {
      ESP_LOGW(TAG, "Your PCA6416A does not support pull-up resistors");
    }
  } else if (flags == gpio::FLAG_OUTPUT) {
    this->update_register_(pin, false, io_dir);
  }
}

bool PCA6416AComponent::read_register_(uint8_t reg, uint8_t *value) {
  if (this->is_failed()) {
    ESP_LOGD(TAG, "Device marked failed");
    return false;
  }

  this->last_error_ = this->read_register(reg, value, 1, true);
  if (this->last_error_ != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "read_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }

  this->status_clear_warning();
  return true;
}

bool PCA6416AComponent::write_register_(uint8_t reg, uint8_t value) {
  if (this->is_failed()) {
    ESP_LOGD(TAG, "Device marked failed");
    return false;
  }

  this->last_error_ = this->write_register(reg, &value, 1, true);
  if (this->last_error_ != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }

  this->status_clear_warning();
  return true;
}

void PCA6416AComponent::update_register_(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  uint8_t bit = pin % 8;
  uint8_t reg_value = 0;
  if (reg_addr == PCA6416A_OUTPUT0) {
    reg_value = this->output_0_;
  } else if (reg_addr == PCA6416A_OUTPUT1) {
    reg_value = this->output_1_;
  } else {
    this->read_register_(reg_addr, &reg_value);
  }

  if (pin_value) {
    reg_value |= 1 << bit;
  } else {
    reg_value &= ~(1 << bit);
  }

  this->write_register_(reg_addr, reg_value);

  if (reg_addr == PCA6416A_OUTPUT0) {
    this->output_0_ = reg_value;
  } else if (reg_addr == PCA6416A_OUTPUT1) {
    this->output_1_ = reg_value;
  }
}

float PCA6416AComponent::get_setup_priority() const { return setup_priority::IO; }

void PCA6416AGPIOPin::setup() { pin_mode(flags_); }
void PCA6416AGPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool PCA6416AGPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void PCA6416AGPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string PCA6416AGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via PCA6416A", pin_);
  return buffer;
}

}  // namespace pca6416a
}  // namespace esphome
