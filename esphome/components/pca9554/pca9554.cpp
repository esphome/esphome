#include "pca9554.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pca9554 {

// for 16 bit expanders, these addresses will be doubled.
const uint8_t INPUT_REG = 0;
const uint8_t OUTPUT_REG = 1;
const uint8_t INVERT_REG = 2;
const uint8_t CONFIG_REG = 3;

static const char *const TAG = "pca9554";

void PCA9554Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PCA9554/PCA9554A...");
  this->reg_width_ = (this->pin_count_ + 7) / 8;
  // Test to see if device exists
  if (!this->read_inputs_()) {
    ESP_LOGE(TAG, "PCA95xx not detected at 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // No polarity inversion
  this->write_register_(INVERT_REG, 0);
  // All inputs at initialization
  this->config_mask_ = 0;
  // Invert mask as the part sees a 1 as an input
  this->write_register_(CONFIG_REG, ~this->config_mask_);
  // All outputs low
  this->output_mask_ = 0;
  this->write_register_(OUTPUT_REG, this->output_mask_);
  // Read the inputs
  this->read_inputs_();
  ESP_LOGD(TAG, "Initialization complete. Warning: %d, Error: %d", this->status_has_warning(),
           this->status_has_error());
}

void PCA9554Component::loop() {
  // The read_inputs_() method will cache the input values from the chip.
  this->read_inputs_();
  // Clear all the previously read flags.
  this->was_previously_read_ = 0x00;
}

void PCA9554Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PCA9554:");
  ESP_LOGCONFIG(TAG, "  I/O Pins: %d", this->pin_count_);
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with PCA9554 failed!");
  }
}

bool PCA9554Component::digital_read(uint8_t pin) {
  // Note: We want to try and avoid doing any I2C bus read transactions here
  // to conserve I2C bus bandwidth. So what we do is check to see if we
  // have seen a read during the time esphome is running this loop. If we have,
  // we do an I2C bus transaction to get the latest value. If we haven't
  // we return a cached value which was read at the time loop() was called.
  if (this->was_previously_read_ & (1 << pin))
    this->read_inputs_();  // Force a read of a new value
  // Indicate we saw a read request for this pin in case a
  // read happens later in the same loop.
  this->was_previously_read_ |= (1 << pin);
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
  uint8_t inputs[2];

  if (this->is_failed()) {
    ESP_LOGD(TAG, "Device marked failed");
    return false;
  }

  this->last_error_ = this->read_register(INPUT_REG * this->reg_width_, inputs, this->reg_width_, true);
  if (this->last_error_ != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "read_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }
  this->status_clear_warning();
  this->input_mask_ = inputs[0];
  if (this->reg_width_ == 2) {
    this->input_mask_ |= inputs[1] << 8;
  }
  return true;
}

bool PCA9554Component::write_register_(uint8_t reg, uint16_t value) {
  uint8_t outputs[2];
  outputs[0] = (uint8_t) value;
  outputs[1] = (uint8_t) (value >> 8);
  this->last_error_ = this->write_register(reg * this->reg_width_, outputs, this->reg_width_, true);
  if (this->last_error_ != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }

  this->status_clear_warning();
  return true;
}

float PCA9554Component::get_setup_priority() const { return setup_priority::IO; }

// Run our loop() method very early in the loop, so that we cache read values before
// before other components call our digital_read() method.
float PCA9554Component::get_loop_priority() const { return 9.0f; }  // Just after WIFI

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
