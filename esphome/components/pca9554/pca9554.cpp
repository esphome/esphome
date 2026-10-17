#include "pca9554.h"
#include "esphome/core/log.h"

namespace esphome::pca9554 {

// for 16 bit expanders, these addresses will be doubled.
static constexpr uint8_t INPUT_REG = 0;
static constexpr uint8_t OUTPUT_REG = 1;
static constexpr uint8_t INVERT_REG = 2;
static constexpr uint8_t CONFIG_REG = 3;

#ifdef USE_PCA9554_PCAL
// PCAL registers; on 16 bit parts the offset from 0x40 is doubled as well.
static constexpr uint8_t OUTPUT_DRIVE_0 = 0x40;
static constexpr uint8_t OUTPUT_DRIVE_1 = 0x41;
static constexpr uint8_t INPUT_LATCH = 0x42;
static constexpr uint8_t PUPD_ENABLE = 0x43;
static constexpr uint8_t PUPD_SEL = 0x44;
static constexpr uint8_t INTERRUPT_MASK = 0x45;
#endif

ESPHOME_LOG_TAG(TAG, "pca9554");

/// Register address on the chip: the base registers are spaced by the register width.
static uint8_t register_address(uint8_t reg, size_t reg_width) {
  if (reg < 0x40)
    return reg * reg_width;
  return 0x40 + (reg - 0x40) * reg_width;
}

void PCA9554Component::setup() {
  this->reg_width_ = (this->pin_count_ + 7) / 8;
  // Test to see if device exists
  if (!this->read_inputs_()) {
    ESP_LOGE(TAG, "PCA95xx not detected at 0x%02X", this->address_);
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
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
#ifdef USE_PCA9554_PCAL
  if (this->pcal_)
    this->reset_pcal_registers_();
#endif
  // Read the inputs
  this->read_inputs_();
  ESP_LOGD(TAG, "Initialization complete. Warning: %d, Error: %d", this->status_has_warning(),
           this->status_has_error());

  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->interrupt_pin_->attach_interrupt(&PCA9554Component::gpio_intr, this, gpio::INTERRUPT_FALLING_EDGE);
    // Don't invalidate cache on read — only invalidate when interrupt fires
    this->set_invalidate_on_read_(false);
  }
  // Disable loop until an input pin is configured via pin_mode()
  // For interrupt-driven mode, loop is re-enabled by the ISR
  // For polling mode, loop is re-enabled when pin_mode() registers an input pin
  this->disable_loop();
}

void IRAM_ATTR PCA9554Component::gpio_intr(PCA9554Component *arg) { arg->enable_loop_soon_any_context(); }

void PCA9554Component::loop() {
  // Invalidate the cache so the next digital_read() triggers a fresh I2C read
  this->reset_pin_cache_();
  // Only disable the loop once INT has actually gone HIGH. Input transitions that straddle the
  // I2C read leave INT asserted without re-firing a falling edge, which would strand us with
  // stale state forever; keep looping until the line is released so we self-heal.
  if (this->interrupt_pin_ != nullptr && this->interrupt_pin_->digital_read()) {
    this->disable_loop();
  }
}

void PCA9554Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PCA9554:\n"
                "  I/O Pins: %d",
                this->pin_count_);
#ifdef USE_PCA9554_PCAL
  ESP_LOGCONFIG(TAG, "  PCAL registers: %s", YESNO(this->pcal_));
#endif
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_I2C_DEVICE(this)
}

bool PCA9554Component::digital_read_hw(uint8_t pin) {
  // Read all pins from hardware into input_mask_
  return this->read_inputs_();  // Return true if I2C read succeeded, false on error
}

bool PCA9554Component::digital_read_cache(uint8_t pin) {
  // Return the cached pin state from input_mask_
  return this->input_mask_ & (1 << pin);
}

void PCA9554Component::digital_write_hw(uint8_t pin, bool value) {
  if (value) {
    this->output_mask_ |= (1 << pin);
  } else {
    this->output_mask_ &= ~(1 << pin);
  }
  this->write_register_(OUTPUT_REG, this->output_mask_);
}

void PCA9554Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (flags & gpio::FLAG_INPUT) {
    // Clear mode mask bit
    this->config_mask_ &= ~(1 << pin);
    // Enable polling loop for input pins (not needed for interrupt-driven mode
    // where the ISR handles re-enabling loop)
    if (this->interrupt_pin_ == nullptr) {
      this->enable_loop();
    }
  } else if (flags & gpio::FLAG_OUTPUT) {
    // Set mode mask bit
    this->config_mask_ |= 1 << pin;
  }
  this->write_register_(CONFIG_REG, ~this->config_mask_);

#ifdef USE_PCA9554_PCAL
  // Validation only lets a PCAL pin ask for a pull resistor
  if (this->pcal_) {
    const bool pulled = flags & (gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN);
    this->set_register_bit_(PUPD_ENABLE, pin, pulled);
    if (pulled)
      this->set_register_bit_(PUPD_SEL, pin, flags & gpio::FLAG_PULLUP);
  }
#endif
}

bool PCA9554Component::read_inputs_() {
  uint8_t inputs[2];

  if (this->is_failed()) {
    ESP_LOGD(TAG, "Device marked failed");
    return false;
  }

  this->last_error_ = this->read_register(register_address(INPUT_REG, this->reg_width_), inputs, this->reg_width_);
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
  this->last_error_ = this->write_register(register_address(reg, this->reg_width_), outputs, this->reg_width_);
  if (this->last_error_ != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }

  this->status_clear_warning();
  return true;
}

#ifdef USE_PCA9554_PCAL
bool PCA9554Component::read_register_(uint8_t reg, uint16_t &value) {
  uint8_t data[2] = {0, 0};
  this->last_error_ = this->read_register(register_address(reg, this->reg_width_), data, this->reg_width_);
  if (this->last_error_ != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "read_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }
  this->status_clear_warning();
  value = (data[1] << 8) | data[0];
  return true;
}

bool PCA9554Component::set_register_bit_(uint8_t reg, uint8_t bit, bool value) {
  uint16_t data;
  if (!this->read_register_(reg, data))
    return false;
  if (value) {
    data |= 1U << bit;
  } else {
    data &= ~(1U << bit);
  }
  return this->write_register_(reg, data);
}

void PCA9554Component::reset_pcal_registers_() {
  this->write_register_(PUPD_ENABLE, 0x0000);
  this->write_register_(PUPD_SEL, 0xFFFF);
  this->write_register_(INPUT_LATCH, 0x0000);
  this->write_register_(INTERRUPT_MASK, 0xFFFF);
  this->write_register_(OUTPUT_DRIVE_0, 0xFFFF);
  this->write_register_(OUTPUT_DRIVE_1, 0xFFFF);
}

void PCA9554Component::set_latch(uint8_t pin, bool latch) {
  if (this->pcal_)
    this->set_register_bit_(INPUT_LATCH, pin, latch);
}

void PCA9554Component::set_interrupt(uint8_t pin, bool enabled) {
  // A set mask bit disables the interrupt
  if (this->pcal_)
    this->set_register_bit_(INTERRUPT_MASK, pin, !enabled);
}

void PCA9554Component::set_drive_strength(uint8_t pin, uint8_t strength) {
  if (!this->pcal_)
    return;
  // Two bits per pin: a one byte register holds four pins, a two byte one holds eight
  const uint8_t pins_per_register = 4 * this->reg_width_;
  const uint8_t reg = pin < pins_per_register ? OUTPUT_DRIVE_0 : OUTPUT_DRIVE_1;
  const uint8_t shift = 2 * (pin % pins_per_register);
  uint16_t value;
  if (!this->read_register_(reg, value))
    return;
  value &= ~(3U << shift);
  value |= (strength & 3U) << shift;
  this->write_register_(reg, value);
}
#endif

float PCA9554Component::get_setup_priority() const { return setup_priority::IO; }

void PCA9554GPIOPin::setup() {
  this->pin_mode(this->flags_);
#ifdef USE_PCA9554_PCAL
  this->parent_->set_latch(this->pin_, this->latch_);
  this->parent_->set_interrupt(this->pin_, this->interrupt_);
  this->parent_->set_drive_strength(this->pin_, this->drive_strength_);
#endif
}

void PCA9554GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool PCA9554GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void PCA9554GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
size_t PCA9554GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via PCA9554", this->pin_);
}

}  // namespace esphome::pca9554
