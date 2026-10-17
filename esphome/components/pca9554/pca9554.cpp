#include "pca9554.h"
#include "esphome/core/log.h"

namespace esphome::pca9554 {

// for 16 bit expanders, these addresses will be doubled.
static constexpr uint8_t INPUT_REG = 0;
static constexpr uint8_t OUTPUT_REG = 1;
static constexpr uint8_t INVERT_REG = 2;
static constexpr uint8_t CONFIG_REG = 3;

#ifdef USE_PCA9554_PCAL
// PCAL registers, as offsets from PCAL_BASE spaced by the register width like the ones above.
static constexpr uint8_t PCAL_BASE = 0x40;
static constexpr uint8_t OUTPUT_DRIVE_0 = 0;
static constexpr uint8_t OUTPUT_DRIVE_1 = 1;
static constexpr uint8_t INPUT_LATCH = 2;
static constexpr uint8_t PUPD_ENABLE = 3;
static constexpr uint8_t PUPD_SEL = 4;
static constexpr uint8_t INTERRUPT_MASK = 5;
#endif

ESPHOME_LOG_TAG(TAG, "pca9554");

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
  // The chip keeps these across an MCU reboot
  if (this->pcal_) {
    this->write_pcal_register_(PUPD_ENABLE, this->pupd_enable_);
    this->write_pcal_register_(PUPD_SEL, this->pupd_select_);
    this->write_pcal_register_(INPUT_LATCH, this->latch_);
    this->write_pcal_register_(INTERRUPT_MASK, this->interrupt_mask_);
    this->write_pcal_register_(OUTPUT_DRIVE_0, this->drive_0_);
    this->write_pcal_register_(OUTPUT_DRIVE_1, this->drive_1_);
  }
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
  if (this->pcal_) {
    const uint16_t bit = 1U << pin;
    // A masked pin never asserts INT, so only inputs are unmasked
    this->update_pcal_register_(INTERRUPT_MASK, this->interrupt_mask_, bit, this->config_mask_ & bit);
    this->update_pcal_register_(PUPD_SEL, this->pupd_select_, bit, (flags & gpio::FLAG_PULLUP) ? bit : 0);
    this->update_pcal_register_(PUPD_ENABLE, this->pupd_enable_, bit,
                                (flags & (gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN)) ? bit : 0);
  }
#endif
}

bool PCA9554Component::read_inputs_() {
  uint8_t inputs[2];

  if (this->is_failed()) {
    ESP_LOGD(TAG, "Device marked failed");
    return false;
  }

  const i2c::ErrorCode err = this->read_register(INPUT_REG * this->reg_width_, inputs, this->reg_width_);
  if (err != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "read_register_(): I2C I/O error: %d", (int) err);
    return false;
  }
  this->status_clear_warning();
  this->input_mask_ = inputs[0];
  if (this->reg_width_ == 2) {
    this->input_mask_ |= inputs[1] << 8;
  }
  return true;
}

bool PCA9554Component::write_address_(uint8_t address, uint16_t value) {
  uint8_t outputs[2];
  outputs[0] = (uint8_t) value;
  outputs[1] = (uint8_t) (value >> 8);
  const i2c::ErrorCode err = this->write_register(address, outputs, this->reg_width_);
  if (err != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write_register_(): I2C I/O error: %d", (int) err);
    return false;
  }

  this->status_clear_warning();
  return true;
}

#ifdef USE_PCA9554_PCAL
bool PCA9554Component::write_pcal_register_(uint8_t reg, uint16_t value) {
  return this->write_address_(PCAL_BASE + reg * this->reg_width_, value);
}

void PCA9554Component::update_pcal_register_(uint8_t reg, uint16_t &shadow, uint16_t mask, uint16_t value) {
  const uint16_t updated = (shadow & ~mask) | (value & mask);
  // setup() wrote every shadow, so an unchanged value is already on the chip
  if (updated == shadow)
    return;
  // Keep the old shadow on a failed write so the same request retries it
  if (this->write_pcal_register_(reg, updated))
    shadow = updated;
}

void PCA9554Component::set_drive_strength(uint8_t pin, uint8_t strength) {
  // Two bits per pin: a one byte register holds four pins, a two byte one holds eight
  const uint8_t pins_per_register = this->pin_count_ > 8 ? 8 : 4;
  uint16_t &drive = pin < pins_per_register ? this->drive_0_ : this->drive_1_;
  const uint8_t shift = 2 * (pin % pins_per_register);
  drive = (drive & ~(3U << shift)) | ((strength & 3U) << shift);
}
#endif

float PCA9554Component::get_setup_priority() const { return setup_priority::IO; }

void PCA9554GPIOPin::setup() { this->pin_mode(this->flags_); }
void PCA9554GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool PCA9554GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void PCA9554GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
size_t PCA9554GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via PCA9554", this->pin_);
}

}  // namespace esphome::pca9554
