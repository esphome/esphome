#include "max7321.h"
#include "esphome/core/log.h"

namespace esphome {
namespace max7321 {

static const char *const TAG = "max7321";

/**************************************
 *    MAX7321                         *
 **************************************/
void MAX7321::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MAX7321...");
  uint8_t configuration;
  if (!this->read_bytes_raw(&configuration, 1)) {
    this->mark_failed();
    return;
  }
}

bool MAX7321::digital_read(uint8_t pin) {
  uint8_t value = 0;
  this->read_bytes_raw(&value, 1);
  return (value & (1 << pin));
}

void MAX7321::digital_write(uint8_t pin, bool value) {
  uint8_t configuration;
  this->read_bytes_raw(&configuration, 1);
  if (value) {
    configuration |= (1 << pin);
  } else {
    configuration &= ~(1 << pin);
  }
  this->write_(configuration);
}

void MAX7321::pin_mode(uint8_t pin, gpio::Flags flags) {
  uint8_t configuration;
  this->read_bytes_raw(&configuration, 1);

  if (flags == gpio::FLAG_INPUT) {
    configuration |= (1 << pin);
  } else if (flags == gpio::FLAG_OUTPUT) {
    configuration &= ~(1 << pin);
  }
}

bool MAX7321::write_(uint8_t value) {
  if (this->is_failed())
    return false;

  if (this->write(&value, 1) == 0) {
    return true;
  } else {
    return false;
  }
}

void MAX7321::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX7321");
  LOG_I2C_DEVICE(this);
}

/**************************************
 *    MAX7321GPIOPin                  *
 **************************************/
void MAX7321GPIOPin::setup() { pin_mode(flags_); }
void MAX7321GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool MAX7321GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void MAX7321GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
std::string MAX7321GPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via MAX7321", pin_);
  return buffer;
}

}  // namespace max7321
}  // namespace esphome
