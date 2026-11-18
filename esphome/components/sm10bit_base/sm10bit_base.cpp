#include "sm10bit_base.h"

namespace esphome::sm10bit_base {

static constexpr uint8_t SM10BIT_MODEL_ID = 0xC0;

static constexpr uint8_t SM10BIT_ADDR_STANDBY = 0x0;
static constexpr uint8_t SM10BIT_ADDR_START_3CH = 0x8;
static constexpr uint8_t SM10BIT_ADDR_START_2CH = 0x10;
static constexpr uint8_t SM10BIT_ADDR_START_5CH = 0x18;

static constexpr uint8_t SM10BIT_DELAY = 2;

// Power current values
// HEX | Binary | RGB level | White level | Config value
// 0x0 | 0000   | RGB 10mA  | CW 5mA      | 0
// 0x1 | 0001   | RGB 20mA  | CW 10mA     | 1
// 0x2 | 0010   | RGB 30mA  | CW 15mA     | 2 - Default spec color value
// 0x3 | 0011   | RGB 40mA  | CW 20mA     | 3
// 0x4 | 0100   | RGB 50mA  | CW 25mA     | 4 - Default spec white value
// 0x5 | 0101   | RGB 60mA  | CW 30mA     | 5
// 0x6 | 0110   | RGB 70mA  | CW 35mA     | 6
// 0x7 | 0111   | RGB 80mA  | CW 40mA     | 7
// 0x8 | 1000   | RGB 90mA  | CW 45mA     | 8
// 0x9 | 1001   | RGB 100mA | CW 50mA     | 9
// 0xA | 1010   | RGB 110mA | CW 55mA     | 10
// 0xB | 1011   | RGB 120mA | CW 60mA     | 11
// 0xC | 1100   | RGB 130mA | CW 65mA     | 12
// 0xD | 1101   | RGB 140mA | CW 70mA     | 13
// 0xE | 1110   | RGB 150mA | CW 75mA     | 14
// 0xF | 1111   | RGB 160mA | CW 80mA     | 15

void Sm10BitBase::setup() {
  this->data_pin_->setup();
  this->data_pin_->digital_write(true);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(true);
}

void Sm10BitBase::loop() {
  this->disable_loop();

  const bool color_on = this->pwm_amounts_[0] > 0 || this->pwm_amounts_[1] > 0 || this->pwm_amounts_[2] > 0;
  const bool white_on = this->pwm_amounts_[3] > 0 || this->pwm_amounts_[4] > 0;
  uint8_t addr = SM10BIT_ADDR_START_5CH;
  if (color_on && !white_on) {
    addr = SM10BIT_ADDR_START_3CH;
  } else if (white_on && !color_on) {
    addr = SM10BIT_ADDR_START_2CH;
  }

  uint8_t data[12]{};
  data[0] = SM10BIT_MODEL_ID + addr;
  // Only the max power of the channel groups being driven is sent
  data[1] = (color_on ? this->max_power_color_channels_ << 4 : 0) | (white_on ? this->max_power_white_channels_ : 0);
  for (size_t i = 0; i < this->pwm_amounts_.size(); i++) {
    data[2 + i * 2] = this->pwm_amounts_[i] >> 8;
    data[3 + i * 2] = this->pwm_amounts_[i] & 0xFF;
  }
  this->write_buffer_(data, sizeof(data));

  if (!color_on && !white_on) {
    // All channels are off, so put the chip to sleep
    data[0] = SM10BIT_MODEL_ID + SM10BIT_ADDR_STANDBY;
    this->write_buffer_(data, sizeof(data));
  }
}

void Sm10BitBase::write_channel(uint8_t channel, float state) {
  auto value = static_cast<uint16_t>(state * 0x3FF);
  if (this->pwm_amounts_[channel] != value) {
    this->pwm_amounts_[channel] = value;
    this->enable_loop();
  }
}

void Sm10BitBase::write_bit_(bool value) {
  this->data_pin_->digital_write(value);
  delayMicroseconds(SM10BIT_DELAY);
  this->clock_pin_->digital_write(true);
  delayMicroseconds(SM10BIT_DELAY);
  this->clock_pin_->digital_write(false);
  delayMicroseconds(SM10BIT_DELAY);
}

void Sm10BitBase::write_byte_(uint8_t data) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    this->write_bit_(data & mask);
  }

  // ack bit
  this->data_pin_->pin_mode(gpio::FLAG_INPUT);
  this->clock_pin_->digital_write(true);
  delayMicroseconds(SM10BIT_DELAY);
  this->clock_pin_->digital_write(false);
  delayMicroseconds(SM10BIT_DELAY);
  this->data_pin_->pin_mode(gpio::FLAG_OUTPUT);
}

void Sm10BitBase::write_buffer_(const uint8_t *buffer, uint8_t size) {
  this->data_pin_->digital_write(false);
  delayMicroseconds(SM10BIT_DELAY);
  this->clock_pin_->digital_write(false);
  delayMicroseconds(SM10BIT_DELAY);

  for (uint32_t i = 0; i < size; i++) {
    this->write_byte_(buffer[i]);
  }

  this->clock_pin_->digital_write(true);
  delayMicroseconds(SM10BIT_DELAY);
  this->data_pin_->digital_write(true);
  delayMicroseconds(SM10BIT_DELAY);
}

}  // namespace esphome::sm10bit_base
