#include "pi4ioe5v6408.h"
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

  // No need to clear latched interrupts before attaching the ISR — if INT is
  // already low the ISR fires immediately, loop runs, cache invalidates, and
  // the read clears the latch. One harmless extra read at most.
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->attach_interrupt(&PI4IOE5V6408Component::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
    this->set_invalidate_on_read_(false);
  }
  // Disable loop until an input pin is configured via pin_mode()
  // For interrupt-driven mode, loop is re-enabled by the ISR
  // For polling mode, loop is re-enabled when pin_mode() registers an input pin
  this->disable_loop();
}
void IRAM_ATTR PI4IOE5V6408Component::gpio_intr(PI4IOE5V6408Component *arg) { arg->enable_loop_soon_any_context(); }
void PI4IOE5V6408Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PI4IOE5V6408:");
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
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
    // Enable polling loop for input pins (not needed for interrupt-driven mode
    // where the ISR handles re-enabling loop)
    if (this->interrupt_pin_ == nullptr) {
      this->enable_loop();
    }
  }
  // Write GPIO to enable input mode
  this->write_gpio_modes_();
}

void PI4IOE5V6408Component::loop() {
  this->reset_pin_cache_();
  // Only disable the loop once INT has actually gone HIGH. Input transitions that straddle the
  // I2C read leave INT asserted without re-firing a falling edge, which would strand us with
  // stale state forever; keep looping until the line is released so we self-heal.
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read()) {
    this->disable_loop();
  }
}

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

float PI4IOE5V6408Component::get_setup_priority() const { return setup_priority::IO; }

void PI4IOE5V6408GPIOPin::setup() { this->pin_mode(this->flags_); }
void PI4IOE5V6408GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool PI4IOE5V6408GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void PI4IOE5V6408GPIOPin::digital_write(bool value) {
  this->parent_->digital_write(this->pin_, value != this->inverted_);
}
size_t PI4IOE5V6408GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via PI4IOE5V6408", this->pin_);
}

}  // namespace esphome::pi4ioe5v6408
