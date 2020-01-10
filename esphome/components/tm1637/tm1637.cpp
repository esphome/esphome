#include "tm1637.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace tm1637 {

static const char* TAG = "display.tm1637";
static const uint8_t TM1637_I2C_COMM1 = 0x40;
static const uint8_t TM1637_I2C_COMM2 = 0xC0;
static const uint8_t TM1637_I2C_COMM3 = 0x80;
static const uint8_t minusSegments = 0b01000000;
static const uint8_t TM1637_UNKNOWN_CHAR = 0b11111111;

const uint8_t TM1637_ASCII_TO_RAW[94] PROGMEM = {
    0b00000000,           // ' ', ord 0x20
    0b10110000,           // '!', ord 0x21
    0b00100010,           // '"', ord 0x22
    TM1637_UNKNOWN_CHAR,  // '#', ord 0x23
    TM1637_UNKNOWN_CHAR,  // '$', ord 0x24
    0b01001001,           // '%', ord 0x25
    TM1637_UNKNOWN_CHAR,  // '&', ord 0x26
    0b00000010,           // ''', ord 0x27
    0b01001110,           // '(', ord 0x28
    0b01111000,           // ')', ord 0x29
    0b01000000,           // '*', ord 0x2A
    TM1637_UNKNOWN_CHAR,  // '+', ord 0x2B
    0b00010000,           // ',', ord 0x2C
    0b00000001,           // '-', ord 0x2D
    0b10000000,           // '.', ord 0x2E
    TM1637_UNKNOWN_CHAR,  // '/', ord 0x2F
    0b01111110,           // '0', ord 0x30
    0b00110000,           // '1', ord 0x31
    0b01101101,           // '2', ord 0x32
    0b01111001,           // '3', ord 0x33
    0b00110011,           // '4', ord 0x34
    0b01011011,           // '5', ord 0x35
    0b01011111,           // '6', ord 0x36
    0b01110000,           // '7', ord 0x37
    0b01111111,           // '8', ord 0x38
    0b01110011,           // '9', ord 0x39
    0b01001000,           // ':', ord 0x3A
    0b01011000,           // ';', ord 0x3B
    TM1637_UNKNOWN_CHAR,  // '<', ord 0x3C
    TM1637_UNKNOWN_CHAR,  // '=', ord 0x3D
    TM1637_UNKNOWN_CHAR,  // '>', ord 0x3E
    0b01100101,           // '?', ord 0x3F
    0b01101111,           // '@', ord 0x40
    0b01110111,           // 'A', ord 0x41
    0b00011111,           // 'B', ord 0x42
    0b01001110,           // 'C', ord 0x43
    0b00111101,           // 'D', ord 0x44
    0b01001111,           // 'E', ord 0x45
    0b01000111,           // 'F', ord 0x46
    0b01011110,           // 'G', ord 0x47
    0b00110111,           // 'H', ord 0x48
    0b00110000,           // 'I', ord 0x49
    0b00111100,           // 'J', ord 0x4A
    TM1637_UNKNOWN_CHAR,  // 'K', ord 0x4B
    0b00001110,           // 'L', ord 0x4C
    TM1637_UNKNOWN_CHAR,  // 'M', ord 0x4D
    0b00010101,           // 'N', ord 0x4E
    0b01111110,           // 'O', ord 0x4F
    0b01100111,           // 'P', ord 0x50
    0b11111110,           // 'Q', ord 0x51
    0b00000101,           // 'R', ord 0x52
    0b01011011,           // 'S', ord 0x53
    0b00000111,           // 'T', ord 0x54
    0b00111110,           // 'U', ord 0x55
    0b00111110,           // 'V', ord 0x56
    0b00111111,           // 'W', ord 0x57
    TM1637_UNKNOWN_CHAR,  // 'X', ord 0x58
    0b00100111,           // 'Y', ord 0x59
    0b01101101,           // 'Z', ord 0x5A
    0b01001110,           // '[', ord 0x5B
    TM1637_UNKNOWN_CHAR,  // '\', ord 0x5C
    0b01111000,           // ']', ord 0x5D
    TM1637_UNKNOWN_CHAR,  // '^', ord 0x5E
    0b00001000,           // '_', ord 0x5F
    0b00100000,           // '`', ord 0x60
    0b01110111,           // 'a', ord 0x61
    0b00011111,           // 'b', ord 0x62
    0b00001101,           // 'c', ord 0x63
    0b00111101,           // 'd', ord 0x64
    0b01001111,           // 'e', ord 0x65
    0b01000111,           // 'f', ord 0x66
    0b01011110,           // 'g', ord 0x67
    0b00010111,           // 'h', ord 0x68
    0b00010000,           // 'i', ord 0x69
    0b00111100,           // 'j', ord 0x6A
    TM1637_UNKNOWN_CHAR,  // 'k', ord 0x6B
    0b00001110,           // 'l', ord 0x6C
    TM1637_UNKNOWN_CHAR,  // 'm', ord 0x6D
    0b00010101,           // 'n', ord 0x6E
    0b00011101,           // 'o', ord 0x6F
    0b01100111,           // 'p', ord 0x70
    TM1637_UNKNOWN_CHAR,  // 'q', ord 0x71
    0b00000101,           // 'r', ord 0x72
    0b01011011,           // 's', ord 0x73
    0b00000111,           // 't', ord 0x74
    0b00011100,           // 'u', ord 0x75
    0b00011100,           // 'v', ord 0x76
    TM1637_UNKNOWN_CHAR,  // 'w', ord 0x77
    TM1637_UNKNOWN_CHAR,  // 'x', ord 0x78
    0b00100111,           // 'y', ord 0x79
    TM1637_UNKNOWN_CHAR,  // 'z', ord 0x7A
    0b00110001,           // '{', ord 0x7B
    0b00000110,           // '|', ord 0x7C
    0b00000111,           // '}', ord 0x7D
};

void TM1637Display::setup_pins_() {
  // this->init_internal_(this->get_buffer_length_());
  this->clk_pin_->setup();               // OUTPUT
  this->clk_pin_->digital_write(false);  // LOW
  this->dio_pin_->setup();               // OUTPUT
  this->dio_pin_->digital_write(false);  // LOW
}

void TM1637Display::set_writer(tm1637_writer_t&& writer) { this->writer_ = writer; }
float TM1637Display::get_setup_priority() const { return setup_priority::PROCESSOR; }
void TM1637Display::setBrightness(uint8_t brightness) { m_brightness = (brightness & 0x7) | 0x08; }
void TM1637Display::bitDelay() { delayMicroseconds(m_bitDelay); }
void TM1637Display::start() {
  this->dio_pin_->digital_write(false);
  this->bitDelay();
}
void TM1637Display::stop() {
  this->dio_pin_->digital_write(false);
  this->bitDelay();
  this->clk_pin_->digital_write(true);
  this->bitDelay();
  this->dio_pin_->digital_write(true);
  this->bitDelay();
}
bool TM1637Display::writeByte(uint8_t b) {
  uint8_t data = b;

  // 8 Data Bits
  for (uint8_t i = 0; i < 8; i++) {
    // CLK low
    this->clk_pin_->digital_write(false);
    this->bitDelay();

    // Set data bit
    if (data & 0x01)
      this->dio_pin_->digital_write(true);
    else
      this->clk_pin_->digital_write(false);

    this->bitDelay();

    // CLK high
    this->clk_pin_->digital_write(true);
    this->bitDelay();
    data = data >> 1;
  }

  // Wait for acknowledge
  // CLK to zero
  this->clk_pin_->digital_write(false);
  this->dio_pin_->digital_write(true);
  this->bitDelay();

  // CLK to high
  this->clk_pin_->digital_write(true);
  this->bitDelay();
  uint8_t ack = this->dio_pin_->digital_read();
  if (ack == 0) {
    this->dio_pin_->digital_write(false);
  }

  this->bitDelay();
  this->clk_pin_->digital_write(false);
  this->bitDelay();

  return ack;
}
void TM1637Display::setSegments(const uint8_t segments[], uint8_t length, uint8_t pos) {
  // Write COMM1
  this->start();
  this->writeByte(TM1637_I2C_COMM1);
  this->stop();

  // Write COMM2 + first digit address
  this->start();
  this->writeByte(TM1637_I2C_COMM2 + (pos & 0x03));

  // Write the data bytes
  for (uint8_t k = 0; k < length; k++) {
    this->writeByte(segments[k]);
  }

  this->stop();

  // Write COMM3 + brightness
  this->start();
  this->writeByte(TM1637_I2C_COMM3 + (m_brightness & 0x0f));
  this->stop();
}
void TM1637Display::clear() {
  uint8_t data[] = {0, 0, 0, 0};
  this->setSegments(data);
}
void TM1637Display::showDots(uint8_t dots, uint8_t* digits) {
  for (int i = 0; i < 4; ++i) {
    digits[i] |= (dots & 0x80);
    dots <<= 1;
  }
}
uint8_t TM1637Display::encodeDigit(uint8_t digit) { return TM1637_ASCII_TO_RAW[(digit & 0x0f) + 16]; }
void TM1637Display::showNumberBaseEx(int8_t base, uint16_t num, uint8_t dots, bool leading_zero, uint8_t length,
                                     uint8_t pos) {
  bool negative = false;
  if (base < 0) {
    base = -base;
    negative = true;
  }

  uint8_t digits[4];

  if (num == 0 && !leading_zero) {
    // Singular case - take care separately
    for (uint8_t i = 0; i < (length - 1); i++)
      digits[i] = 0;
    digits[length - 1] = this->encodeDigit(0);
  } else {
    // uint8_t i = length-1;
    // if (negative) {
    //	// Negative number, show the minus sign
    //    digits[i] = minusSegments;
    //	i--;
    //}

    for (int i = length - 1; i >= 0; --i) {
      uint8_t digit = num % base;

      if (digit == 0 && num == 0 && leading_zero == false)
        // Leading zero is blank
        digits[i] = 0;
      else
        digits[i] = this->encodeDigit(digit);

      if (digit == 0 && num == 0 && negative) {
        digits[i] = minusSegments;
        negative = false;
      }

      num /= base;
    }

    if (dots != 0) {
      this->showDots(dots, digits);
    }
  }
  this->setSegments(digits, length, pos);
}
void TM1637Display::showNumberDecEx(int num, uint8_t dots, bool leading_zero, uint8_t length, uint8_t pos) {
  this->showNumberBaseEx(num < 0 ? -10 : 10, num < 0 ? -num : num, dots, leading_zero, length, pos);
}
void TM1637Display::showNumberDec(int num, bool leading_zero, uint8_t length, uint8_t pos) {
  this->showNumberDecEx(num, 0, leading_zero, length, pos);
}

}  // namespace tm1637
}  // namespace esphome
