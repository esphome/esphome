#include "sx1509.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::sx1509 {

ESPHOME_LOG_TAG(TAG, "sx1509");

void SX1509Component::setup() {
  ESP_LOGV(TAG, "  Resetting devices");
  if (!this->setup_check(this->write_byte(REG_RESET, 0x12)))
    return;
  this->write_byte(REG_RESET, 0x34);

  uint16_t data;
  if (!this->setup_check(this->read_byte_16(REG_INTERRUPT_MASK_A, &data)))
    return;
  if (!this->setup_check(data == 0xFF00, LOG_STR("Unexpected reset value")))
    return;
  this->clock_(INTERNAL_CLOCK_2MHZ, 1, 0, this->led_driver_frequency_);
  delayMicroseconds(500);
  if (this->has_keypad_)
    this->setup_keypad_();
}

void SX1509Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SX1509:\n"
                "  LED driver frequency: %u Hz",
                7840u >> (this->led_driver_frequency_ - 1));
  LOG_I2C_DEVICE(this);
}

void SX1509Component::loop() {
  // Reset cache at the start of each loop
  this->reset_pin_cache_();

  if (this->has_keypad_) {
    if (millis() - this->last_loop_timestamp_ < min_loop_period_)
      return;
    this->last_loop_timestamp_ = millis();
    uint16_t key_data = this->read_key_data();
    for (auto *binary_sensor : this->keypad_binary_sensors_)
      binary_sensor->process(key_data);
    if (this->keys_ == nullptr)
      return;
    if (key_data == 0) {
      this->last_key_ = 0;
      return;
    }
    int row, col;
    for (row = 0; row < 8; row++) {
      if (key_data & (1 << row))
        break;
    }
    for (col = 8; col < 16; col++) {
      if (key_data & (1 << col))
        break;
    }
    col -= 8;
    if (row >= this->rows_ || col >= this->cols_)  // a partial read can leave only a row or a column bit
      return;
    uint8_t key = progmem_read_byte(&this->keys_[row * this->cols_ + col]);
    if (key == this->last_key_)
      return;
    this->last_key_ = key;
    ESP_LOGV(TAG, "row %d, col %d, key '%c'", row, col, key);
#ifdef SX1509_ON_KEY_COUNT
    this->key_automations_.trigger(key);
#endif
    this->send_key_(key);
  }
}

bool SX1509Component::digital_read_hw(uint8_t pin) {
  // Always read all pins when any input pin is accessed
  return this->read_byte_16(REG_DATA_B, &this->input_mask_);
}

bool SX1509Component::digital_read_cache(uint8_t pin) {
  // Return cached value for input pins, false for output pins
  if (this->ddr_mask_ & (1 << pin)) {
    return (this->input_mask_ & (1 << pin)) != 0;
  }
  return false;
}

void SX1509Component::digital_write_hw(uint8_t pin, bool bit_value) {
  const uint16_t bit = 1 << pin;
  if ((~this->ddr_mask_) & bit) {
    assign_bits(this->output_state_, bit, bit_value);
    // Outputs take the shadow state, inputs keep what was read. Unlike update_register_16_(), a failed read
    // still writes, since every output bit comes from the shadow.
    uint16_t data = 0;
    this->read_byte_16(REG_DATA_B, &data);
    this->write_byte_16(REG_DATA_B, (data & this->ddr_mask_) | (this->output_state_ & ~this->ddr_mask_));
  }
}

void SX1509Component::pin_mode(uint8_t pin, gpio::Flags flags) {
  ESP_LOGI(TAG, "Configuring pin %u with flags %x", pin, flags);

  const uint16_t bit = 1 << pin;

  this->read_byte_16(REG_DIR_B, &this->ddr_mask_);
  if (flags & gpio::FLAG_OUTPUT) {
    // Always disable input buffer
    this->update_register_16_(REG_INPUT_DISABLE_B, bit, bit);

    if (flags & gpio::FLAG_OPEN_DRAIN) {
      // Pullup must be disabled for open drain mode
      this->update_register_16_(REG_PULL_UP_B, bit, 0);
      this->update_register_16_(REG_OPEN_DRAIN_B, bit, bit);
      ESP_LOGD(TAG, "Open drain output mode set for %u", pin);
    } else {
      ESP_LOGD(TAG, "Output Mode for %u", pin);
    }
  } else {
    ESP_LOGD(TAG, "Input Mode for %u", pin);

    // Always enable input buffer
    this->update_register_16_(REG_INPUT_DISABLE_B, bit, 0);
    this->update_register_16_(REG_PULL_UP_B, bit, (flags & gpio::FLAG_PULLUP) ? bit : 0);
    this->update_register_16_(REG_PULL_DOWN_B, bit, (flags & gpio::FLAG_PULLDOWN) ? bit : 0);
  }
  // A set direction bit makes the pin an input
  assign_bits(this->ddr_mask_, bit, !(flags & gpio::FLAG_OUTPUT));
  this->write_byte_16(REG_DIR_B, this->ddr_mask_);
}

void SX1509Component::setup_led_driver(uint8_t pin) {
  const uint16_t bit = 1 << pin;

  this->update_register_16_(REG_INPUT_DISABLE_B, bit, bit);

  this->ddr_mask_ &= ~bit;  // 0=output
  this->write_byte_16(REG_DIR_B, this->ddr_mask_);

  this->update_register_16_(REG_LED_DRIVER_ENABLE_B, bit, bit);

  this->output_state_ &= ~bit;
  this->update_register_16_(REG_DATA_B, bit, 0);
}

bool SX1509Component::update_register_16_(uint8_t a_register, uint16_t mask, uint16_t value) {
  uint16_t data;
  return this->read_byte_16(a_register, &data) && this->write_byte_16(a_register, (data & ~mask) | (value & mask));
}

void SX1509Component::clock_(uint8_t osc_source, uint8_t osc_pin_function, uint8_t osc_freq_out, uint8_t osc_divider) {
  osc_source = (osc_source & 0b11) << 5;           // 2-bit value, bits 6:5
  osc_pin_function = (osc_pin_function & 1) << 4;  // 1-bit value bit 4
  osc_freq_out = (osc_freq_out & 0b1111);          // 4-bit value, bits 3:0
  uint8_t reg_clock = osc_source | osc_pin_function | osc_freq_out;
  this->write_byte(REG_CLOCK, reg_clock);

  // LED driver clock ClkX (bits 6:4); bits 7 and 3 clear select linear mode for banks B and A
  this->write_byte(REG_MISC, (osc_divider & 0b111) << 4);
}

void SX1509Component::setup_keypad_() {
  uint8_t temp_byte = 0;

  // setup row/col pins for INPUT OUTPUT
  this->read_byte_16(REG_DIR_B, &this->ddr_mask_);
  for (int i = 0; i < this->rows_; i++)
    this->ddr_mask_ &= ~(1 << i);
  for (int i = 8; i < (8 + this->cols_); i++)
    this->ddr_mask_ |= (1 << i);
  this->write_byte_16(REG_DIR_B, this->ddr_mask_);

  const uint8_t row_bits = (1 << this->rows_) - 1;
  this->update_register(REG_OPEN_DRAIN_A, row_bits, row_bits);

  const uint8_t col_bits = (1 << this->cols_) - 1;
  this->update_register(REG_PULL_UP_B, col_bits, col_bits);

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
  temp_byte = ((this->rows_ - 1) & 0b111) << 3;  // 0 = off, 0b001 = 2 rows, 0b111 = 8 rows, etc.
  temp_byte |= (this->cols_ - 1) & 0b111;        // 0b000 = 1 column, ob111 = 8 columns, etc.
  this->write_byte(REG_KEY_CONFIG_2, temp_byte);
}

uint16_t SX1509Component::read_key_data() {
  uint16_t key_data = 0;
  this->read_byte_16(REG_KEY_DATA_1, &key_data);
  return (0xFFFF ^ key_data);
}

void SX1509Component::set_debounce_config_(uint8_t config_value) {
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
  this->update_register_16_(REG_DEBOUNCE_ENABLE_B, 1 << pin, 1 << pin);
}

void SX1509Component::set_debounce_pin_(uint8_t pin) { set_debounce_enable_(pin); }

void SX1509Component::set_debounce_keypad_(uint8_t time, uint8_t num_rows, uint8_t num_cols) {
  set_debounce_time_(time);
  for (uint16_t i = 0; i < num_rows; i++)
    set_debounce_pin_(i);
  for (uint16_t i = 0; i < num_cols; i++)
    set_debounce_pin_(i + 8);
}

}  // namespace esphome::sx1509

ESPHOME_EXPANDER_PIN_SUMMARY(esphome::sx1509::SX1509Component, "%u via sx1509")
