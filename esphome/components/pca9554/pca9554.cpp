#include "pca9554.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pca9554 {

// for 16 bit expanders, these addresses will be doubled.
const uint8_t INPUT_REG = 0;
const uint8_t OUTPUT_REG = 1;
const uint8_t INVERT_REG = 2;
const uint8_t CONFIG_REG = 3;

const uint8_t OUTPUT_DRIVE_0 = 0x40;
const uint8_t OUTPUT_DRIVE_1 = 0x41;
const uint8_t INPUT_LATCH = 0x42;
const uint8_t PUPD_ENABLE = 0x43;
const uint8_t PUPD_SEL = 0x44;
const uint8_t INTERRUPT_MASK = 0x45;
const uint8_t INTERRUPT_STATUS = 0x46;

const uint8_t OUTPUT_PORT_CONFIG = 0x4F;

const uint8_t CAPABILITY_PULLUP = 0x02U;
const uint8_t CAPABILITY_PULLDOWN = 0x04U;
const uint8_t CAPABILITY_LATCH = 0x08U;
const uint8_t CAPABILITY_INTERRUPT = 0x10U;
const uint8_t CAPABILITY_DRIVE_STRENGTH = 0x20U;

static const char *const TAG = "pca9554";

void PCA9554Component::setup() {
  this->reg_width_ = (this->pin_count_ + 7) / 8;
  // Test to see if device exists
  if (!this->read_inputs_()) {
    ESP_LOGE(TAG, "PCA95xx not detected at 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // No polarity inversion
  this->write_register_(INVERT_REG, 0);
  // Set all pins to input
  this->write_register_(CONFIG_REG, 0xFFFF);
  // Set all outputs low
  this->write_register_(OUTPUT_REG, 0x0000);
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
  if (this->interrupt_pin_ != nullptr) {
    // Interrupt-driven: disable loop until next interrupt fires
    this->disable_loop();
  }
}

void PCA9554Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "PCA9554:\n"
                "  I/O Pins: %d",
                this->pin_count_);
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_I2C_DEVICE(this)
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

bool PCA9554Component::digital_read_hw(uint8_t pin) {
  // Read all pins from hardware into input_states_
  return this->read_inputs_();  // Return true if I2C read succeeded, false on error
}

bool PCA9554Component::digital_read_cache(uint8_t pin) {
  // Return the cached pin state from input_states_
  return this->input_states_ & (1 << pin);
}

void PCA9554Component::digital_write_hw(uint8_t pin, bool value) { this->set_register_bit_(OUTPUT_REG, pin, value); }

void PCA9554Component::do_stuff() {
  uint8_t i;
  i = 13;
  uint16_t reg_val = 0;
  uint8_t OD_val = 0;

  ESP_LOGD("pca9554", "Capability: %d", this->capability_);

  this->read_register_(CONFIG_REG, reg_val);
  ESP_LOGD("pca9554", "CONFIG_REG: 0x%04X", reg_val);
  this->read_register_(OUTPUT_DRIVE_0, reg_val);
  ESP_LOGD("pca9554", "OUTPUT_DRIVE_0: 0x%04X", reg_val);
  this->read_register_(OUTPUT_DRIVE_1, reg_val);
  ESP_LOGD("pca9554", "OUTPUT_DRIVE_1: 0x%04X", reg_val);
  this->read_register_(INPUT_LATCH, reg_val);
  ESP_LOGD("pca9554", "INPUT_LATCH: 0x%04X", reg_val);
  this->read_register_(PUPD_ENABLE, reg_val);
  ESP_LOGD("pca9554", "PUPD_ENABLE: 0x%04X", reg_val);
  this->read_register_(PUPD_SEL, reg_val);
  ESP_LOGD("pca9554", "PUPD_SEL: 0x%04X", reg_val);
  this->read_register_(INTERRUPT_MASK, reg_val);
  ESP_LOGD("pca9554", "INTERRUPT_MASK: 0x%04X", reg_val);
  /*if (i > 7) {
    OD_val = (reg_val>>(2U*(i-8U)))&(3U);
  } else {
    OD_val = (reg_val>>(2U*i))&(3U);
  }

  ESP_LOGD("pca9554", "OD of pin %d: %d", i, OD_val);
  OD_val = OD_val + 1;
  if(OD_val > 3) {
    OD_val = 0;
  }
  ESP_LOGD("pca9554", "Setting value to %d", OD_val);
  this->set_drive_strength(i, OD_val);

  this->read_register_(OUTPUT_DRIVE_1, reg_val);
  ESP_LOGD("pca9554", "OUTPUT_DRIVE_1: 0x%04X", reg_val);

  // this->pin_mode(i, this->saved_flags_[i]);

  //i = 4;
  //ESP_LOGD("pca9554", "set pin mode for %d with flags %d", i, gpio::FLAG_OUTPUT);
  //this->pin_mode(i, gpio::FLAG_OUTPUT);*/
}

void PCA9554Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  // Note: no checks are done here to validate that the flags are legitimate.
  //  This should have happened in the python code.
  if (pin > (this->pin_count_ - 1)) {
    ESP_LOGE(TAG, "Invalid pin %d", pin);
    return;
  }

  if (flags & gpio::FLAG_INPUT) {
    // Set the pin as input (1)
    this->set_register_bit_(CONFIG_REG, pin, true);
    // Enable polling loop for input pins (not needed for interrupt-driven mode
    // where the ISR handles re-enabling loop)
    if (this->interrupt_pin_ == nullptr) {
      this->enable_loop();
    }
  } else if (flags & gpio::FLAG_OUTPUT) {
    // Set the pin as output (0)
    this->set_register_bit_(CONFIG_REG, pin, false);
  }

  if ((this->capability_ & CAPABILITY_PULLUP) | (this->capability_ & CAPABILITY_PULLDOWN)) {
    if (flags & gpio::FLAG_PULLUP) {
      this->set_register_bit_(PUPD_ENABLE, pin, true);
      this->set_register_bit_(PUPD_SEL, pin, true);
    } else if (flags & gpio::FLAG_PULLDOWN) {
      this->set_register_bit_(PUPD_ENABLE, pin, true);
      this->set_register_bit_(PUPD_SEL, pin, false);
    } else {
      this->set_register_bit_(PUPD_ENABLE, pin, false);
    }
  }
}

bool PCA9554Component::read_inputs_() {
  if (this->is_failed()) {
    ESP_LOGD(TAG, "Device marked failed");
    return false;
  }

  uint16_t input_values = 0;
  if (this->read_register_(INPUT_REG, input_values)) {
    // Register read successfully
    this->input_states_ = input_values;
    return true;
  }
  return false;
}

uint8_t PCA9554Component::get_register_address_(uint8_t reg) {
  // Determine the actual register address.
  if (reg == OUTPUT_PORT_CONFIG) {
    // OUTPUT_PORT_CONFIG does not change between device types
    return OUTPUT_PORT_CONFIG;
  } else if (reg <= CONFIG_REG) {
    // Double the first four registers for devices with >8 pins
    return reg * this->reg_width_;
  } else if ((reg >= OUTPUT_DRIVE_0) && (reg <= INTERRUPT_STATUS)) {
    // For devices with >8 pins, these registers start at 0x40 and should be doubled
    return 0x40 + ((reg - 0x40) * this->reg_width_);
  } else {
    // Invald register, 0xFF is not a valid register address, so it can be used to indicate an error.
    return 0xFF;
  }
}

bool PCA9554Component::write_register_(uint8_t reg, uint16_t value) {
  uint8_t outputs[2];
  outputs[0] = (uint8_t) value;
  outputs[1] = (uint8_t) (value >> 8);
  uint8_t register_to_write = this->get_register_address_(reg);

  if (register_to_write == 0xFF) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write_register_(): Attempt to write to invalid register 0x%02X", reg);
    return false;
  }

  this->last_error_ = this->write_register(register_to_write, outputs, this->reg_width_);
  if (this->last_error_ != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }

  this->status_clear_warning();
  return true;
}

bool PCA9554Component::read_register_(uint8_t reg, uint16_t &register_value) {
  uint8_t register_to_read = this->get_register_address_(reg);
  uint8_t reg_data[2];
  reg_data[0] = 0;
  reg_data[1] = 0;

  if (register_to_read == 0xFF) {
    this->status_set_warning();
    ESP_LOGE(TAG, "read_register_(): Attempt to read from invalid register 0x%02X", reg);
    return false;
  }

  this->last_error_ = this->read_register(register_to_read, reg_data, this->reg_width_);
  if (this->last_error_ != i2c::ERROR_OK) {
    this->status_set_warning();
    ESP_LOGE(TAG, "read_register_(): I2C I/O error: %d", (int) this->last_error_);
    return false;
  }

  this->status_clear_warning();
  register_value = (reg_data[1] << 8) + reg_data[0];
  return true;
}

bool PCA9554Component::set_register_bit_(uint8_t reg, uint8_t bit, bool value) {
  uint16_t reg_data;

  // Get current register value
  if (!(this->read_register_(reg, reg_data))) {
    return false;
  }

  // Modify bit
  if (value) {
    // Set the bit to 1
    reg_data |= 1U << bit;
  } else {
    // Set the bit to 0
    reg_data &= ~(1U << bit);
  }

  // Write modified register
  if (!(this->write_register_(reg, reg_data))) {
    return false;
  }
  return true;
}

float PCA9554Component::get_setup_priority() const { return setup_priority::IO; }

void PCA9554Component::set_latch(uint8_t pin, bool latch_state) {
  if (this->capability_ & CAPABILITY_LATCH) {
    if (pin > (this->pin_count_ - 1)) {
      ESP_LOGE(TAG, "Invalid pin %d", pin);
      return;
    }

    this->set_register_bit_(INPUT_LATCH, pin, latch_state);
  }
}

void PCA9554Component::set_interrupt(uint8_t pin, bool interrupt_state) {
  if (this->capability_ & CAPABILITY_INTERRUPT) {
    if (pin > (this->pin_count_ - 1)) {
      ESP_LOGE(TAG, "Invalid pin %d", pin);
      return;
    }

    // In this register, a zero indicates the interrupt is unmasked.
    this->set_register_bit_(INTERRUPT_MASK, pin, !interrupt_state);
  }
}

// Sets drive strength
//  0: 1/4 strength
//  1: 1/2 strength
//  2: 3/4 strength
//  3: full strength
void PCA9554Component::set_drive_strength(uint8_t pin, uint16_t strength_to_set) {
  if (this->capability_ & CAPABILITY_DRIVE_STRENGTH) {
    uint8_t register_to_modify = OUTPUT_DRIVE_0;
    uint16_t reg_value = 0;

    if (pin > (this->pin_count_ - 1)) {
      ESP_LOGE(TAG, "Invalid pin %d", pin);
      return;
    }
    if (strength_to_set > 0x03) {
      ESP_LOGE(TAG, "Invalid drive strength %d", strength_to_set);
      return;
    }

    if (pin > 7) {
      register_to_modify = OUTPUT_DRIVE_1;
      pin = pin - 8;
    }

    if (!this->read_register_(register_to_modify, reg_value)) {
      return;
    }
    reg_value &= ~(3U << (2U * pin));
    reg_value |= strength_to_set << (2U * pin);
    this->write_register_(register_to_modify, reg_value);
  }
}

// Set `state_open_drain` to true to turn on open drain for that bank.
void PCA9554Component::set_open_drain(uint8_t bank_to_set, bool state_open_drain) {
  if (((this->pin_count_ > 8) && (bank_to_set <= 1)) || ((this->pin_count_ <= 8) && (bank_to_set == 0))) {
    this->set_register_bit_(OUTPUT_PORT_CONFIG, bank_to_set, state_open_drain);
    return;
  }
  ESP_LOGE(TAG, "Invalid bank %d", bank_to_set);
}

// Returns 0xFFFF if a read error occurs.
uint16_t PCA9554Component::get_interrupt_status() {
  uint16_t status_to_report;
  if (this->read_register_(INTERRUPT_STATUS, status_to_report)) {
    return status_to_report;
  }
  return 0xFFFF;
}

void PCA9554GPIOPin::setup() {
  this->pin_mode(flags_);
  this->parent_->set_latch(this->pin_, this->latch_);
  this->parent_->set_interrupt(this->pin_, this->interrupt_);
  this->parent_->set_drive_strength(this->pin_, this->drive_strength_);
}

void PCA9554GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool PCA9554GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void PCA9554GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
size_t PCA9554GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via PCA9554", this->pin_);
}

}  // namespace pca9554
}  // namespace esphome
