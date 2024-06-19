#include "sx1509.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sx1509 {

static const char *const TAG = "sx1509";

void SX1509Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SX1509Component...");

  ESP_LOGV(TAG, "  Resetting devices...");
  if (!this->write_byte(REG_RESET, 0x12)) {
    this->mark_failed();
    return;
  }
  this->write_byte(REG_RESET, 0x34);

  uint16_t data;
  if (!this->read_byte_16(REG_INTERRUPT_MASK_A, &data)) {
    this->mark_failed();
    return;
  }
  if (data != 0xFF00) {
    this->mark_failed();
    return;
  }
  clock_(INTERNAL_CLOCK_2MHZ);
  delayMicroseconds(500);
  if (this->has_keypad_)
    this->setup_keypad_();
}

void SX1509Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SX1509:");
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setting up SX1509 failed!");
  }
  LOG_I2C_DEVICE(this);
}

void SX1509Component::loop() {
  if (this->has_keypad_) {
    if (millis() - this->last_loop_timestamp_ < min_loop_period_)
      return;
    this->last_loop_timestamp_ = millis();
    uint16_t key_data = this->read_key_data();
    for (auto *binary_sensor : this->keypad_binary_sensors_)
      binary_sensor->process(key_data);
  }
}

bool SX1509Component::digital_read(uint8_t pin) {
  if (this->ddr_mask_ & (1 << pin)) {
    uint16_t temp_reg_data;
    if (!this->read_byte_16(REG_DATA_B, &temp_reg_data))
      return false;
    if (temp_reg_data & (1 << pin))
      return true;
  }
  return false;
}

void SX1509Component::digital_write(uint8_t pin, bool bit_value) {
  if ((~this->ddr_mask_) & (1 << pin)) {
    // If the pin is an output, write high/low
    uint16_t temp_reg_data = 0;
    this->read_byte_16(REG_DATA_B, &temp_reg_data);
    if (bit_value) {
      output_state_ |= (1 << pin);  // set bit in shadow register
    } else {
      output_state_ &= ~(1 << pin);  // reset bit shadow register
    }
    for (uint16_t b = 0x8000; b; b >>= 1) {
      if ((~ddr_mask_) & b) {  // transfer bits of outputs, but don't mess with inputs
        if (output_state_ & b) {
          temp_reg_data |= b;
        } else {
          temp_reg_data &= ~b;
        }
      }
    }
    this->write_byte_16(REG_DATA_B, temp_reg_data);
  }
}

void SX1509Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  ESP_LOGI(TAG, "Configuring pin %u with flags %x", pin, flags);

  uint16_t temp_word = 0;

  this->read_byte_16(REG_DIR_B, &this->ddr_mask_);
  if (flags & gpio::FLAG_OUTPUT) {
    // Always disable input buffer
    this->read_byte_16(REG_INPUT_DISABLE_B, &temp_word);
    temp_word |= (1 << pin);
    this->write_byte_16(REG_INPUT_DISABLE_B, temp_word);

    if (flags & gpio::FLAG_OPEN_DRAIN) {
      // Pullup must be disabled for open drain mode
      this->read_byte_16(REG_PULL_UP_B, &temp_word);
      temp_word &= ~(1 << pin);
      this->write_byte_16(REG_PULL_UP_B, temp_word);
      this->read_byte_16(REG_OPEN_DRAIN_B, &temp_word);
      temp_word |= (1 << pin);
      this->write_byte_16(REG_OPEN_DRAIN_B, temp_word);
      ESP_LOGD(TAG, "Open drain output mode set for %u", pin);
    } else {
      ESP_LOGD(TAG, "Output Mode for %u", pin);
    }

    // Set direction to output
    this->ddr_mask_ &= ~(1 << pin);
    this->write_byte_16(REG_DIR_B, this->ddr_mask_);
  } else {
    ESP_LOGD(TAG, "Input Mode for %u", pin);

    // Always enable input buffer
    this->read_byte_16(REG_INPUT_DISABLE_B, &temp_word);
    temp_word &= ~(1 << pin);
    this->write_byte_16(REG_INPUT_DISABLE_B, temp_word);

    // Pullup
    this->read_byte_16(REG_PULL_UP_B, &temp_word);
    if (flags & gpio::FLAG_PULLUP) {
      temp_word |= (1 << pin);
    } else {
      temp_word &= ~(1 << pin);
    }
    this->write_byte_16(REG_PULL_UP_B, temp_word);

    // Pulldown
    this->read_byte_16(REG_PULL_DOWN_B, &temp_word);
    if (flags & gpio::FLAG_PULLDOWN) {
      temp_word |= (1 << pin);
    } else {
      temp_word &= ~(1 << pin);
    }
    this->write_byte_16(REG_PULL_DOWN_B, temp_word);

    // Set direction to input
    this->ddr_mask_ |= (1 << pin);
    this->write_byte_16(REG_DIR_B, this->ddr_mask_);
  }
}

void SX1509Component::setup_led_driver(uint8_t pin) {
  uint16_t temp_word = 0;
  uint8_t temp_byte = 0;

  this->read_byte_16(REG_INPUT_DISABLE_B, &temp_word);
  temp_word |= (1 << pin);
  this->write_byte_16(REG_INPUT_DISABLE_B, temp_word);

  this->ddr_mask_ &= ~(1 << pin);  // 0=output
  this->write_byte_16(REG_DIR_B, this->ddr_mask_);

  this->read_byte(REG_CLOCK, &temp_byte);
  temp_byte |= (1 << 6);   // Internal 2MHz oscillator part 1 (set bit 6)
  temp_byte &= ~(1 << 5);  // Internal 2MHz oscillator part 2 (clear bit 5)
  this->write_byte(REG_CLOCK, temp_byte);

  this->read_byte(REG_MISC, &temp_byte);
  temp_byte &= ~(1 << 7);  // set linear mode bank B
  temp_byte &= ~(1 << 3);  // set linear mode bank A
  temp_byte |= 0x70;       // Frequency of the LED Driver clock ClkX of all IOs:
  this->write_byte(REG_MISC, temp_byte);

  this->read_byte_16(REG_LED_DRIVER_ENABLE_B, &temp_word);
  temp_word |= (1 << pin);
  this->write_byte_16(REG_LED_DRIVER_ENABLE_B, temp_word);

  this->read_byte_16(REG_DATA_B, &temp_word);
  temp_word &= ~(1 << pin);
  output_state_ &= ~(1 << pin);
  this->write_byte_16(REG_DATA_B, temp_word);
}

void SX1509Component::clock_(uint8_t osc_source, uint8_t osc_pin_function, uint8_t osc_freq_out, uint8_t osc_divider) {
  osc_source = (osc_source & 0b11) << 5;           // 2-bit value, bits 6:5
  osc_pin_function = (osc_pin_function & 1) << 4;  // 1-bit value bit 4
  osc_freq_out = (osc_freq_out & 0b1111);          // 4-bit value, bits 3:0
  uint8_t reg_clock = osc_source | osc_pin_function | osc_freq_out;
  this->write_byte(REG_CLOCK, reg_clock);

  osc_divider = clamp<uint8_t>(osc_divider, 1, 7u);
  this->clk_x_ = 2000000;
  osc_divider = (osc_divider & 0b111) << 4;  // 3-bit value, bits 6:4

  uint8_t reg_misc = 0;
  this->read_byte(REG_MISC, &reg_misc);
  reg_misc &= ~(0b111 << 4);
  reg_misc |= osc_divider;
  this->write_byte(REG_MISC, reg_misc);
}

void SX1509Component::setup_keypad_() {
  uint8_t temp_byte = 0;

  // setup row/col pins for INPUT OUTPUT
  this->read_byte_16(REG_DIR_B, &this->ddr_mask_);
  for (int i = 0; i < this->rows_; i++)
    this->ddr_mask_ &= ~(1 << i);
  for (int i = 8; i < (this->cols_ * 2); i++)
    this->ddr_mask_ |= (1 << i);
  this->write_byte_16(REG_DIR_B, this->ddr_mask_);

  this->read_byte(REG_OPEN_DRAIN_A, &temp_byte);
  for (int i = 0; i < this->rows_; i++)
    temp_byte |= (1 << i);
  this->write_byte(REG_OPEN_DRAIN_A, temp_byte);

  this->read_byte(REG_PULL_UP_B, &temp_byte);
  for (int i = 0; i < this->cols_; i++)
    temp_byte |= (1 << i);
  this->write_byte(REG_PULL_UP_B, temp_byte);

  if (debounce_time_ >= scan_time_) {
    debounce_time_ = scan_time_ >> 1;  // Force debounce_time to be less than scan_time
  }
  set_debounce_keypad_(debounce_time_, rows_, cols_);
  uint8_t scan_time_bits = 0;
  for (uint8_t i = 7; i > 0; i--) {
    if (scan_time_ & (1 << i)) {
      scan_time_bits = i;
      break;
    }
  }
  scan_time_bits &= 0b111;  // Scan time is bits 2:0
  temp_byte = sleep_time_ | scan_time_bits;
  this->write_byte(REG_KEY_CONFIG_1, temp_byte);
  rows_ = (rows_ - 1) & 0b111;  // 0 = off, 0b001 = 2 rows, 0b111 = 8 rows, etc.
  cols_ = (cols_ - 1) & 0b111;  // 0b000 = 1 column, ob111 = 8 columns, etc.
  this->write_byte(REG_KEY_CONFIG_2, (rows_ << 3) | cols_);
}

uint16_t SX1509Component::read_key_data() {
  uint16_t key_data = 0;
  this->read_byte_16(REG_KEY_DATA_1, &key_data);
  return (0xFFFF ^ key_data);
}

void SX1509Component::set_debounce_config_(uint8_t config_value) {
  // First make sure clock is configured
  uint8_t temp_byte = 0;
  this->read_byte(REG_MISC, &temp_byte);
  temp_byte |= (1 << 4);  // Just default to no divider if not set
  this->write_byte(REG_MISC, temp_byte);
  this->read_byte(REG_CLOCK, &temp_byte);
  temp_byte |= (1 << 6);  // default to internal osc.
  this->write_byte(REG_CLOCK, temp_byte);

  config_value &= 0b111;  // 3-bit value
  this->write_byte(REG_DEBOUNCE_CONFIG, config_value);
}

void SX1509Component::set_debounce_time_(uint8_t time) {
  uint8_t config_value = 0;

  for (int i = 7; i >= 0; i--) {
    if (time & (1 << i)) {
      config_value = i + 1;
      break;
    }
  }
  config_value = clamp<uint8_t>(config_value, 0, 7);

  set_debounce_config_(config_value);
}

void SX1509Component::set_debounce_enable_(uint8_t pin) {
  uint16_t debounce_enable = 0;
  this->read_byte_16(REG_DEBOUNCE_ENABLE_B, &debounce_enable);
  debounce_enable |= (1 << pin);
  this->write_byte_16(REG_DEBOUNCE_ENABLE_B, debounce_enable);
}

void SX1509Component::set_debounce_pin_(uint8_t pin) { set_debounce_enable_(pin); }

void SX1509Component::set_debounce_keypad_(uint8_t time, uint8_t num_rows, uint8_t num_cols) {
  set_debounce_time_(time);
  for (uint16_t i = 0; i < num_rows; i++)
    set_debounce_pin_(i);
  for (uint16_t i = 0; i < (8 + num_cols); i++)
    set_debounce_pin_(i);
}

}  // namespace sx1509
}  // namespace esphome
