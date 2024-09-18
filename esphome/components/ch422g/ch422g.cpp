#include "ch422g.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ch422g {

static const uint8_t CH422G_REG_MODE = 0x24;
static const uint8_t CH422G_IO_OE = 0x01;  // enables output mode on 0-7
static const uint8_t CH422G_IO_OD = 0x04;  // enables open drain mode on 8-11
static const uint8_t CH422G_REG_IN = 0x26;
static const uint8_t CH422G_REG_OUT = 0x38;
static const uint8_t CH422G_REG_OD_OUT = 0x23;

static const char *const TAG = "ch422g";

void CH422GComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CH422G...");
  // Test to see if device exists
  if (!this->read_inputs_()) {
    ESP_LOGE(TAG, "CH422G not detected at 0x%02X", this->address_);
    this->mark_failed();
    return;
  }
  // set outputs before mode
  this->set_mode_(this->mode_value_);
  this->write_outputs_();

  ESP_LOGCONFIG(TAG, "Initialization complete. Warning: %d, Error: %d", this->status_has_warning(),
                this->status_has_error());
}

void CH422GComponent::loop() {
  // Clear all the previously read flags.
  this->pin_read_flags_ = 0x00;
}

void CH422GComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CH422G:");
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with CH422G failed!");
  }
}

void CH422GComponent::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (pin <= 8 && flags & gpio::FLAG_OUTPUT) {
    this->mode_value_ |= CH422G_IO_OE;
  }
  if (pin >= 8 && flags & gpio::FLAG_OPEN_DRAIN) {
    this->mode_value_ |= CH422G_IO_OD;
  }
}

bool CH422GComponent::digital_read(uint8_t pin) {
  if (this->pin_read_flags_ == 0 || this->pin_read_flags_ & (1 << pin)) {
    // Read values on first access or in case it's being read again in the same loop
    this->read_inputs_();
  }

  this->pin_read_flags_ |= (1 << pin);
  return (this->input_bits_ & (1 << pin)) != 0;
}

void CH422GComponent::digital_write(uint8_t pin, bool value) {
  if (value) {
    this->output_bits_ |= (1 << pin);
  } else {
    this->output_bits_ &= ~(1 << pin);
  }
  this->write_outputs_();
}

bool CH422GComponent::read_inputs_() {
  if (this->is_failed()) {
    return false;
  }
  uint8_t result;
  if (this->mode_value_ & CH422G_IO_OE) {
    this->set_mode_(this->mode_value_ & ~CH422G_IO_OE);
    result = this->read_reg_(CH422G_REG_IN);
    this->set_mode_(this->mode_value_);
  } else {
    result = this->read_reg_(CH422G_REG_IN);
  }
  this->input_bits_ = result;
  this->status_clear_warning();
  return true;
}
bool CH422GComponent::write_reg_(uint8_t reg, uint8_t value) {
  auto err = this->bus_->write(reg, &value, 1);
  if (err != i2c::ERROR_OK) {
    this->status_set_warning(str_sprintf("write failed for register 0x%X, error %d", reg, err).c_str());
    return false;
  }
  this->status_clear_warning();
  return true;
}

uint8_t CH422GComponent::read_reg_(uint8_t reg) {
  uint8_t value;
  auto err = this->bus_->read(reg, &value, 1);
  if (err != i2c::ERROR_OK) {
    this->status_set_warning(str_sprintf("read failed for register 0x%X, error %d", reg, err).c_str());
    return 0;
  }
  return value;
}

bool CH422GComponent::set_mode_(uint8_t mode) { return this->write_reg_(CH422G_REG_MODE, mode); }

bool CH422GComponent::write_outputs_() {
  return this->write_reg_(CH422G_REG_OUT, static_cast<uint8_t>(this->output_bits_)) &&
         this->write_reg_(CH422G_REG_OD_OUT, static_cast<uint8_t>(this->output_bits_ >> 8));
}

float CH422GComponent::get_setup_priority() const { return setup_priority::IO; }

// Run our loop() method very early in the loop, so that we cache read values
// before other components call our digital_read() method.
float CH422GComponent::get_loop_priority() const { return 9.0f; }  // Just after WIFI

void CH422GGPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool CH422GGPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) ^ this->inverted_; }

void CH422GGPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value ^ this->inverted_); }
std::string CH422GGPIOPin::dump_summary() const { return str_sprintf("EXIO%u via CH422G", pin_); }
void CH422GGPIOPin::set_flags(gpio::Flags flags) {
  flags_ = flags;
  this->parent_->pin_mode(this->pin_, flags);
}

}  // namespace ch422g
}  // namespace esphome
