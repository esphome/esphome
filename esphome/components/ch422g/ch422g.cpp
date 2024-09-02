#include "ch422g.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ch422g {

const uint8_t CH422G_REG_IN = 0x26;
const uint8_t CH422G_REG_OUT = 0x38;
const uint8_t OUT_REG_DEFAULT_VAL = 0xdf;

static const char *const TAG = "ch422g";

void CH422GComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CH422G...");
  // Test to see if device exists
  if (!this->read_inputs_()) {
    ESP_LOGE(TAG, "CH422G not detected at 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // restore defaults over whatever got saved on last boot
  if (!this->restore_value_) {
    this->write_output_(OUT_REG_DEFAULT_VAL);
  }

  ESP_LOGD(TAG, "Initialization complete. Warning: %d, Error: %d", this->status_has_warning(),
           this->status_has_error());
}

void CH422GComponent::loop() {
  // Clear all the previously read flags.
  this->pin_read_cache_ = 0x00;
}

void CH422GComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CH422G:");
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with CH422G failed!");
  }
}

// ch422g doesn't have any flag support (needs docs?)
void CH422GComponent::pin_mode(uint8_t pin, gpio::Flags flags) {}

bool CH422GComponent::digital_read(uint8_t pin) {
  if (this->pin_read_cache_ == 0 || this->pin_read_cache_ & (1 << pin)) {
    // Read values on first access or in case it's being read again in the same loop
    this->read_inputs_();
  }

  this->pin_read_cache_ |= (1 << pin);
  return this->state_mask_ & (1 << pin);
}

void CH422GComponent::digital_write(uint8_t pin, bool value) {
  if (value) {
    this->write_output_(this->state_mask_ | (1 << pin));
  } else {
    this->write_output_(this->state_mask_ & ~(1 << pin));
  }
}

bool CH422GComponent::read_inputs_() {
  if (this->is_failed()) {
    return false;
  }

  uint8_t temp = 0;
  if ((this->last_error_ = this->read(&temp, 1)) != esphome::i2c::ERROR_OK) {
    this->status_set_warning(str_sprintf("read_inputs_(): I2C I/O error: %d", (int) this->last_error_).c_str());
    return false;
  }

  uint8_t output = 0;
  if ((this->last_error_ = this->bus_->read(CH422G_REG_IN, &output, 1)) != esphome::i2c::ERROR_OK) {
    this->status_set_warning(str_sprintf("read_inputs_(): I2C I/O error: %d", (int) this->last_error_).c_str());
    return false;
  }

  this->state_mask_ = output;
  this->status_clear_warning();

  return true;
}

bool CH422GComponent::write_output_(uint8_t value) {
  const uint8_t temp = 1;
  if ((this->last_error_ = this->write(&temp, 1, false)) != esphome::i2c::ERROR_OK) {
    this->status_set_warning(str_sprintf("write_output_(): I2C I/O error: %d", (int) this->last_error_).c_str());
    return false;
  }

  uint8_t write_mask = value;
  if ((this->last_error_ = this->bus_->write(CH422G_REG_OUT, &write_mask, 1)) != esphome::i2c::ERROR_OK) {
    this->status_set_warning(
        str_sprintf("write_output_(): I2C I/O error: %d for write_mask: %d", (int) this->last_error_, (int) write_mask)
            .c_str());
    return false;
  }

  this->state_mask_ = value;
  this->status_clear_warning();
  return true;
}

float CH422GComponent::get_setup_priority() const { return setup_priority::IO; }

// Run our loop() method very early in the loop, so that we cache read values
// before other components call our digital_read() method.
float CH422GComponent::get_loop_priority() const { return 9.0f; }  // Just after WIFI

void CH422GGPIOPin::setup() { pin_mode(flags_); }
void CH422GGPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool CH422GGPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }

void CH422GGPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string CH422GGPIOPin::dump_summary() const { return str_sprintf("EXIO%u via CH422G", pin_); }

}  // namespace ch422g
}  // namespace esphome
