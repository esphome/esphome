#include "pi4ioe5v6408.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pi4ioe5v6408 {

static const uint8_t PI4IOE5V6408_REGISTER_DEVICE_ID = 0x01;
static const uint8_t PI4IOE5V6408_REGISTER_IO_DIR = 0x03;
static const uint8_t PI4IOE5V6408_REGISTER_OUT_SET = 0x05;
static const uint8_t PI4IOE5V6408_REGISTER_OUT_HIGH_IMPEDENCE = 0x07;
static const uint8_t PI4IOE5V6408_REGISTER_IN_DEFAULT_STATE = 0x09;
static const uint8_t PI4IOE5V6408_REGISTER_PULL_ENABLE = 0x0B;
static const uint8_t PI4IOE5V6408_REGISTER_PULL_SELECT = 0x0D;
static const uint8_t PI4IOE5V6408_REGISTER_IN_STATE = 0x0F;
static const uint8_t PI4IOE5V6408_REGISTER_INTERRUPT_ENABLE_MASK = 0x11;
static const uint8_t PI4IOE5V6408_REGISTER_INTERRUPT_STATUS = 0x13;

static const char *const TAG = "pi4ioe5v6408";

void PI4IOE5V6408Component::setup() {
  if (this->reset_) {
    this->reg(PI4IOE5V6408_REGISTER_DEVICE_ID) |= 0b00000001;
    this->reg(PI4IOE5V6408_REGISTER_OUT_HIGH_IMPEDENCE) = 0b00000000;
  } else {
    if (!this->read_gpio_modes_()) {
      this->mark_failed();
      ESP_LOGE(TAG, "Failed to read GPIO modes");
      return;
    }
    if (!this->read_gpio_outputs_()) {
      this->mark_failed();
      ESP_LOGE(TAG, "Failed to read GPIO outputs");
      return;
    }
  }
}
void PI4IOE5V6408Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PI4IOE5V6408:");
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}
void PI4IOE5V6408Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (flags & gpio::FLAG_OUTPUT) {
    // Set mode mask bit
    this->mode_mask_ |= 1 << pin;
  } else if (flags & gpio::FLAG_INPUT) {
    // Clear mode mask bit
    this->mode_mask_ &= ~(1 << pin);
    if (flags & gpio::FLAG_PULLUP) {
      this->pull_up_down_mask_ |= 1 << pin;
      this->pull_enable_mask_ |= 1 << pin;
    } else if (flags & gpio::FLAG_PULLDOWN) {
      this->pull_up_down_mask_ &= ~(1 << pin);
      this->pull_enable_mask_ |= 1 << pin;
    }
  }
  // Write GPIO to enable input mode
  this->write_gpio_modes_();
}

void PI4IOE5V6408Component::loop() { this->reset_pin_cache_(); }

bool PI4IOE5V6408Component::read_gpio_outputs_() {
  if (this->is_failed())
    return false;

  uint8_t data;
  if (!this->read_byte(PI4IOE5V6408_REGISTER_OUT_SET, &data)) {
    this->status_set_warning(LOG_STR("Failed to read output register"));
    return false;
  }
  this->output_mask_ = data;
  this->status_clear_warning();
  return true;
}

bool PI4IOE5V6408Component::read_gpio_modes_() {
  if (this->is_failed())
    return false;

  uint8_t data;
  if (!this->read_byte(PI4IOE5V6408_REGISTER_IO_DIR, &data)) {
    this->status_set_warning(LOG_STR("Failed to read GPIO modes"));
    return false;
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "Read GPIO modes: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(data));
#endif
  this->mode_mask_ = data;
  this->status_clear_warning();
  return true;
}

bool PI4IOE5V6408Component::digital_read_hw(uint8_t pin) {
  if (this->is_failed())
    return false;

  uint8_t data;
  if (!this->read_byte(PI4IOE5V6408_REGISTER_IN_STATE, &data)) {
    this->status_set_warning(LOG_STR("Failed to read GPIO state"));
    return false;
  }
  this->input_mask_ = data;
  this->status_clear_warning();
  return true;
}

void PI4IOE5V6408Component::digital_write_hw(uint8_t pin, bool value) {
  if (this->is_failed())
    return;

  if (value) {
    this->output_mask_ |= (1 << pin);
  } else {
    this->output_mask_ &= ~(1 << pin);
  }
  if (!this->write_byte(PI4IOE5V6408_REGISTER_OUT_SET, this->output_mask_)) {
    this->status_set_warning(LOG_STR("Failed to write output register"));
    return;
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "Wrote GPIO output: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(this->output_mask_));
#endif
  this->status_clear_warning();
}

bool PI4IOE5V6408Component::write_gpio_modes_() {
  if (this->is_failed())
    return false;

  if (!this->write_byte(PI4IOE5V6408_REGISTER_IO_DIR, this->mode_mask_)) {
    this->status_set_warning(LOG_STR("Failed to write GPIO modes"));
    return false;
  }
  if (!this->write_byte(PI4IOE5V6408_REGISTER_PULL_SELECT, this->pull_up_down_mask_)) {
    this->status_set_warning(LOG_STR("Failed to write GPIO pullup/pulldown"));
    return false;
  }
  if (!this->write_byte(PI4IOE5V6408_REGISTER_PULL_ENABLE, this->pull_enable_mask_)) {
    this->status_set_warning(LOG_STR("Failed to write GPIO pull enable"));
    return false;
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG,
           "Wrote GPIO modes: 0b" BYTE_TO_BINARY_PATTERN "\n"
           "Wrote GPIO pullup/pulldown: 0b" BYTE_TO_BINARY_PATTERN "\n"
           "Wrote GPIO pull enable: 0b" BYTE_TO_BINARY_PATTERN,
           BYTE_TO_BINARY(this->mode_mask_), BYTE_TO_BINARY(this->pull_up_down_mask_),
           BYTE_TO_BINARY(this->pull_enable_mask_));
#endif
  this->status_clear_warning();
  return true;
}

bool PI4IOE5V6408Component::digital_read_cache(uint8_t pin) { return (this->input_mask_ & (1 << pin)); }

float PI4IOE5V6408Component::get_setup_priority() const { return setup_priority::IO; }

void PI4IOE5V6408GPIOPin::setup() { this->pin_mode(this->flags_); }
void PI4IOE5V6408GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool PI4IOE5V6408GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void PI4IOE5V6408GPIOPin::digital_write(bool value) {
  this->parent_->digital_write(this->pin_, value != this->inverted_);
}
std::string PI4IOE5V6408GPIOPin::dump_summary() const { return str_sprintf("%u via PI4IOE5V6408", this->pin_); }

}  // namespace pi4ioe5v6408
}  // namespace esphome
