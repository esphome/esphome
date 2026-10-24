#include "pi4ioe5v6408.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::pi4ioe5v6408 {

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

ESPHOME_LOG_TAG(TAG, "pi4ioe5v6408");

void PI4IOE5V6408Component::setup() {
  if (this->reset_) {
    this->reg(PI4IOE5V6408_REGISTER_DEVICE_ID) |= 0b00000001;
    this->reg(PI4IOE5V6408_REGISTER_OUT_HIGH_IMPEDENCE) = 0b00000000;
  } else {
    if (!this->read_gpio_modes_()) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      ESP_LOGE(TAG, "Failed to read GPIO modes");
      return;
    }
    if (!this->read_gpio_outputs_()) {
      this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
      ESP_LOGE(TAG, "Failed to read GPIO outputs");
      return;
    }
  }

  this->setup_interrupt_pin_();
}
void PI4IOE5V6408Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PI4IOE5V6408:");
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_I2C_DEVICE(this)
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
    this->enable_polling_loop_();
  }
  // Write GPIO to enable input mode
  this->write_gpio_modes_();
}

bool PI4IOE5V6408Component::read_gpio_outputs_() {
  if (this->is_failed())
    return false;

  uint8_t data{0};
  if (!this->status_comm_result(this->read_byte(PI4IOE5V6408_REGISTER_OUT_SET, &data),
                                LOG_STR("Failed to read output register")))
    return false;
  this->output_mask_ = data;
  return true;
}

bool PI4IOE5V6408Component::read_gpio_modes_() {
  if (this->is_failed())
    return false;

  uint8_t data{0};
  if (!this->status_comm_result(this->read_byte(PI4IOE5V6408_REGISTER_IO_DIR, &data),
                                LOG_STR("Failed to read GPIO modes")))
    return false;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "Read GPIO modes: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(data));
#endif
  this->mode_mask_ = data;
  return true;
}

bool PI4IOE5V6408Component::digital_read_hw(uint8_t pin) {
  if (this->is_failed())
    return false;

  uint8_t data{0};
  if (!this->status_comm_result(this->read_byte(PI4IOE5V6408_REGISTER_IN_STATE, &data),
                                LOG_STR("Failed to read GPIO state")))
    return false;
  this->input_mask_ = data;
  return true;
}

void PI4IOE5V6408Component::digital_write_hw(uint8_t pin, bool value) {
  if (this->is_failed())
    return;

  assign_bits(this->output_mask_, 1 << pin, value);
  if (!this->status_comm_result(this->write_byte(PI4IOE5V6408_REGISTER_OUT_SET, this->output_mask_),
                                LOG_STR("Failed to write output register")))
    return;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG, "Wrote GPIO output: 0b" BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(this->output_mask_));
#endif
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
  // Enable interrupts for input pins when interrupt pin is configured
  // (input pins have mode_mask_ bit cleared)
  if (this->interrupt_pin_ != nullptr &&
      !this->write_byte(PI4IOE5V6408_REGISTER_INTERRUPT_ENABLE_MASK, static_cast<uint8_t>(~this->mode_mask_))) {
    this->status_set_warning(LOG_STR("Failed to write interrupt enable mask"));
    return false;
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG,
           "Wrote GPIO config:\n"
           "  modes: 0b" BYTE_TO_BINARY_PATTERN "\n"
           "  pullup/pulldown: 0b" BYTE_TO_BINARY_PATTERN "\n"
           "  pull enable: 0b" BYTE_TO_BINARY_PATTERN,
           BYTE_TO_BINARY(this->mode_mask_), BYTE_TO_BINARY(this->pull_up_down_mask_),
           BYTE_TO_BINARY(this->pull_enable_mask_));
#endif
  this->status_clear_warning();
  return true;
}

bool PI4IOE5V6408Component::digital_read_cache(uint8_t pin) { return (this->input_mask_ & (1 << pin)); }

}  // namespace esphome::pi4ioe5v6408

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::pi4ioe5v6408::PI4IOE5V6408Component, "%u via PI4IOE5V6408")
