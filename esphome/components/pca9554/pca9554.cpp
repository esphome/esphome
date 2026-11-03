#include "pca9554.h"
#include "esphome/core/helpers.h"
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
  if (this->pcal_ && !this->write_pcal_shadows_()) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
    return;
  }
#endif
  // Read the inputs
  this->read_inputs_();
  ESP_LOGD(TAG, "Initialization complete. Warning: %d, Error: %d", this->status_has_warning(),
           this->status_has_error());

  this->setup_interrupt_pin_();
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
  assign_bits(this->output_mask_, 1 << pin, value);
#ifdef USE_PCA9554_PCAL
  if (this->write_register_(OUTPUT_REG, this->output_mask_))
    this->retry_pcal_shadows_();  // an output-only hub never reads, so it retries here
#else
  this->write_register_(OUTPUT_REG, this->output_mask_);
#endif
}

void PCA9554Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  if (flags & gpio::FLAG_INPUT) {
    // Clear mode mask bit
    this->config_mask_ &= ~(1 << pin);
    this->enable_polling_loop_();
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
    const bool pull = flags & (gpio::FLAG_PULLUP | gpio::FLAG_PULLDOWN);
    // The select bit means nothing while the pull is disabled
    if (pull)
      this->update_pcal_register_(PUPD_SEL, this->pupd_select_, bit, (flags & gpio::FLAG_PULLUP) ? bit : 0);
    this->update_pcal_register_(PUPD_ENABLE, this->pupd_enable_, bit, pull ? bit : 0);
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
  if (!this->status_comm_result(err == i2c::ERROR_OK)) {
    ESP_LOGE(TAG, "read_register_(): I2C I/O error: %d", (int) err);
    return false;
  }
  this->input_mask_ = inputs[0];
  if (this->reg_width_ == 2) {
    this->input_mask_ |= inputs[1] << 8;
  }
#ifdef USE_PCA9554_PCAL
  // The bus answers again, so retry the settings a failed write left off the chip
  this->retry_pcal_shadows_();
#endif
  return true;
}

bool PCA9554Component::write_address_(uint8_t address, uint16_t value) {
  uint8_t outputs[2];
  outputs[0] = (uint8_t) value;
  outputs[1] = (uint8_t) (value >> 8);
  const i2c::ErrorCode err = this->write_register(address, outputs, this->reg_width_);
  if (!this->status_comm_result(err == i2c::ERROR_OK)) {
    ESP_LOGE(TAG, "write_register(): I2C I/O error: %d", (int) err);
    return false;
  }
  return true;
}

#ifdef USE_PCA9554_PCAL
bool PCA9554Component::write_pcal_register_(uint8_t reg, uint16_t value) {
  if (this->write_address_(PCAL_BASE + reg * this->reg_width_, value))
    return true;
  this->pcal_dirty_ = true;
  return false;
}

bool PCA9554Component::write_pcal_shadows_() {
  this->pcal_dirty_ = false;
  // Every write runs so one failure does not hide the others; each failure sets pcal_dirty_ again
  bool ok = this->write_pcal_register_(PUPD_ENABLE, this->pupd_enable_);
  ok &= this->write_pcal_register_(PUPD_SEL, this->pupd_select_);
  ok &= this->write_pcal_register_(INPUT_LATCH, this->latch_);
  ok &= this->write_pcal_register_(INTERRUPT_MASK, this->interrupt_mask_);
  ok &= this->write_pcal_register_(OUTPUT_DRIVE_0, static_cast<uint16_t>(this->drive_));
  ok &= this->write_pcal_register_(OUTPUT_DRIVE_1, static_cast<uint16_t>(this->drive_ >> (8 * this->reg_width_)));
  return ok;
}

void PCA9554Component::update_pcal_register_(uint8_t reg, uint16_t &shadow, uint16_t mask, uint16_t value) {
  const uint16_t updated = (shadow & ~mask) | (value & mask);
  // The shadows hold the wanted state; an unchanged value is already on the chip unless a write failed
  if (updated == shadow && !this->pcal_dirty_)
    return;
  shadow = updated;
  this->write_pcal_register_(reg, shadow);
}
#endif

}  // namespace esphome::pca9554

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::pca9554::PCA9554Component, "%u via PCA9554")
