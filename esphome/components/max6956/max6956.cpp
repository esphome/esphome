#include "max6956.h"
#include "esphome/core/log.h"

namespace esphome {
namespace max6956 {

static const char *const TAG = "max6956";

/// Masks for MAX6956 Configuration register
const uint32_t MASK_TRANSITION_DETECTION = 0x80;
const uint32_t MASK_INDIVIDUAL_CURRENT = 0x40;
const uint32_t MASK_NORMAL_OPERATION = 0x01;

const uint32_t MASK_1PORT_VALUE = 0x03;
const uint32_t MASK_PORT_CONFIG = 0x03;
const uint8_t MASK_CONFIG_CURRENT = 0x40;
const uint8_t MASK_CURRENT_PIN = 0x0F;

/**************************************
 *    MAX6956                         *
 **************************************/
void MAX6956::setup() {
  uint8_t configuration;
  if (!this->read_reg_(MAX6956_CONFIGURATION, &configuration)) {
    this->mark_failed();
    return;
  }

  write_brightness_global();
  write_brightness_mode();

  /** TO DO : read transition detection in yaml
      TO DO : read indivdual current in yaml **/
  this->read_reg_(MAX6956_CONFIGURATION, &configuration);
  ESP_LOGD(TAG, "Initial reg[0x%.2X]=0x%.2X", MAX6956_CONFIGURATION, configuration);
  configuration = configuration | MASK_NORMAL_OPERATION;
  this->write_reg_(MAX6956_CONFIGURATION, configuration);

  ESP_LOGCONFIG(TAG, "Enabling normal operation");
  ESP_LOGD(TAG, "setup reg[0x%.2X]=0x%.2X", MAX6956_CONFIGURATION, configuration);
}

void MAX6956::loop() {
  // Invalidate cache at the start of each loop
  this->reset_pin_cache_();
}

bool MAX6956::digital_read_hw(uint8_t pin) {
  // MAX6956 pins start at MAX6956_MIN
  if (pin < MAX6956_MIN || pin > MAX6956_MAX) {
    return false;
  }

  // Calculate bank index based on the base class view (no offset adjustment)
  uint8_t bank_index = pin / MAX6956_BANK_SIZE;

  static const uint8_t BANK_REGS[4] = {
      MAX6956_4PORTS_4_7,    // Bank 0: 4 ports 4-7 (bits D0-D3, D4-D7 read as 0)
      MAX6956_8PORTS_8_15,   // Bank 1: 8 ports 8-15 (bits D0-D7)
      MAX6956_8PORTS_16_23,  // Bank 2: 8 ports 16-23 (bits D0-D7)
      MAX6956_8PORTS_24_31,  // Bank 3: 8 ports 24-31 (bits D0-D7)
  };

  // Read the appropriate register
  uint8_t value = 0;
  if (!this->read_reg_(BANK_REGS[bank_index], &value)) {
    return false;
  }

  // Store in cache with proper alignment
  if (bank_index == 0) {
    // Special case for bank 0: pins 4-7 are in bits D0-D3, shift them to bits 4-7
    this->input_banks_[0] = value << MAX6956_BANK0_SHIFT;
  } else {
    // Banks 1-3 map directly
    this->input_banks_[bank_index] = value;
  }

  return true;
}

bool MAX6956::digital_read_cache(uint8_t pin) {
  // MAX6956 pins start at MAX6956_MIN
  if (pin < MAX6956_MIN || pin > MAX6956_MAX) {
    return false;
  }

  // Use the base class's view of banks (no offset adjustment)
  uint8_t bank_index = pin / MAX6956_BANK_SIZE;
  uint8_t bit_position = pin % MAX6956_BANK_SIZE;

  return (this->input_banks_[bank_index] & (1 << bit_position)) != 0;
}

void MAX6956::digital_write_hw(uint8_t pin, bool value) {
  uint8_t reg_addr = MAX6956_1PORT_VALUE_START + pin;
  this->write_reg_(reg_addr, value);
}

void MAX6956::pin_mode(uint8_t pin, gpio::Flags flags) {
  uint8_t reg_addr = MAX6956_PORT_CONFIG_START + (pin - MAX6956_MIN) / 4;
  uint8_t config = 0;
  uint8_t shift = 2 * (pin % 4);
  MAX6956GPIOMode mode = MAX6956_INPUT;

  if (flags == gpio::FLAG_INPUT) {
    mode = MAX6956GPIOMode::MAX6956_INPUT;
  } else if (flags == (gpio::FLAG_INPUT | gpio::FLAG_PULLUP)) {
    mode = MAX6956GPIOMode::MAX6956_INPUT_PULLUP;
  } else if (flags == gpio::FLAG_OUTPUT) {
    mode = MAX6956GPIOMode::MAX6956_OUTPUT;
  }

  this->read_reg_(reg_addr, &config);
  config &= ~(MASK_PORT_CONFIG << shift);
  config |= (mode << shift);
  this->write_reg_(reg_addr, config);
}

void MAX6956::pin_mode(uint8_t pin, max6956::MAX6956GPIOFlag flags) {
  uint8_t reg_addr = MAX6956_PORT_CONFIG_START + (pin - MAX6956_MIN) / 4;
  uint8_t config = 0;
  uint8_t shift = 2 * (pin % 4);
  MAX6956GPIOMode mode = MAX6956GPIOMode::MAX6956_LED;

  if (flags == max6956::FLAG_LED) {
    mode = MAX6956GPIOMode::MAX6956_LED;
  }

  this->read_reg_(reg_addr, &config);
  config &= ~(MASK_PORT_CONFIG << shift);
  config |= (mode << shift);
  this->write_reg_(reg_addr, config);
}

void MAX6956::set_brightness_global(uint8_t current) {
  if (current > 15) {
    ESP_LOGE(TAG, "Global brightness out off range (%u)", current);
    return;
  }
  global_brightness_ = current;
}

void MAX6956::write_brightness_global() { this->write_reg_(MAX6956_GLOBAL_CURRENT, global_brightness_); }

void MAX6956::set_brightness_mode(max6956::MAX6956CURRENTMODE brightness_mode) { brightness_mode_ = brightness_mode; };

void MAX6956::write_brightness_mode() {
  uint8_t reg_addr = MAX6956_CONFIGURATION;
  uint8_t config = 0;

  this->read_reg_(reg_addr, &config);
  config &= ~MASK_CONFIG_CURRENT;
  config |= brightness_mode_ << 6;
  this->write_reg_(reg_addr, config);
}

void MAX6956::set_pin_brightness(uint8_t pin, float brightness) {
  if (pin < MAX6956_MIN || pin > MAX6956_MAX)
    return;
  uint8_t reg_addr = MAX6956_CURRENT_START + (pin - MAX6956_MIN) / 2;
  uint8_t config = 0;
  uint8_t shift = 4 * (pin % 2);
  uint8_t bright = roundf(brightness * 15);

  if (prev_bright_[pin - MAX6956_MIN] == bright)
    return;

  prev_bright_[pin - MAX6956_MIN] = bright;

  this->read_reg_(reg_addr, &config);
  config &= ~(MASK_CURRENT_PIN << shift);
  config |= (bright << shift);
  this->write_reg_(reg_addr, config);
}

bool MAX6956::read_reg_(uint8_t reg, uint8_t *value) {
  if (this->is_failed())
    return false;

  return this->read_byte(reg, value);
}

bool MAX6956::write_reg_(uint8_t reg, uint8_t value) {
  if (this->is_failed())
    return false;

  return this->write_byte(reg, value);
}

void MAX6956::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX6956");

  if (brightness_mode_ == MAX6956CURRENTMODE::GLOBAL) {
    ESP_LOGCONFIG(TAG,
                  "  Current mode: global\n"
                  "  Brightness: %u",
                  global_brightness_);
  } else {
    ESP_LOGCONFIG(TAG, "  Current mode: segment");
  }
}

/**************************************
 *    MAX6956GPIOPin                  *
 **************************************/
void MAX6956GPIOPin::setup() { pin_mode(flags_); }
void MAX6956GPIOPin::pin_mode(gpio::Flags flags) { this->parent_->pin_mode(this->pin_, flags); }
bool MAX6956GPIOPin::digital_read() { return this->parent_->digital_read(this->pin_) != this->inverted_; }
void MAX6956GPIOPin::digital_write(bool value) { this->parent_->digital_write(this->pin_, value != this->inverted_); }
size_t MAX6956GPIOPin::dump_summary(char *buffer, size_t len) const {
  return buf_append_printf(buffer, len, 0, "%u via Max6956", this->pin_);
}

}  // namespace max6956
}  // namespace esphome
