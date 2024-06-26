#include "max7219.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace max7219 {

static const char *const TAG = "max7219";

static const uint8_t MAX7219_REGISTER_NOOP = 0x00;
static const uint8_t MAX7219_REGISTER_DECODE_MODE = 0x09;
static const uint8_t MAX7219_REGISTER_INTENSITY = 0x0A;
static const uint8_t MAX7219_REGISTER_SCAN_LIMIT = 0x0B;
static const uint8_t MAX7219_REGISTER_SHUTDOWN = 0x0C;
static const uint8_t MAX7219_REGISTER_TEST = 0x0F;
static const uint8_t MAX7219_UNKNOWN_CHAR = 0b11111111;

const uint8_t MAX7219_ASCII_TO_RAW[95] PROGMEM = {
    0b00000000,            // ' ', ord 0x20
    0b10110000,            // '!', ord 0x21
    0b00100010,            // '"', ord 0x22
    MAX7219_UNKNOWN_CHAR,  // '#', ord 0x23
    MAX7219_UNKNOWN_CHAR,  // '$', ord 0x24
    0b01001001,            // '%', ord 0x25
    MAX7219_UNKNOWN_CHAR,  // '&', ord 0x26
    0b00000010,            // ''', ord 0x27
    0b01001110,            // '(', ord 0x28
    0b01111000,            // ')', ord 0x29
    0b01000000,            // '*', ord 0x2A
    MAX7219_UNKNOWN_CHAR,  // '+', ord 0x2B
    0b00010000,            // ',', ord 0x2C
    0b00000001,            // '-', ord 0x2D
    0b10000000,            // '.', ord 0x2E
    MAX7219_UNKNOWN_CHAR,  // '/', ord 0x2F
    0b01111110,            // '0', ord 0x30
    0b00110000,            // '1', ord 0x31
    0b01101101,            // '2', ord 0x32
    0b01111001,            // '3', ord 0x33
    0b00110011,            // '4', ord 0x34
    0b01011011,            // '5', ord 0x35
    0b01011111,            // '6', ord 0x36
    0b01110000,            // '7', ord 0x37
    0b01111111,            // '8', ord 0x38
    0b01111011,            // '9', ord 0x39
    0b01001000,            // ':', ord 0x3A
    0b01011000,            // ';', ord 0x3B
    MAX7219_UNKNOWN_CHAR,  // '<', ord 0x3C
    0b00001001,            // '=', ord 0x3D
    MAX7219_UNKNOWN_CHAR,  // '>', ord 0x3E
    0b01100101,            // '?', ord 0x3F
    0b01101111,            // '@', ord 0x40
    0b01110111,            // 'A', ord 0x41
    0b00011111,            // 'B', ord 0x42
    0b01001110,            // 'C', ord 0x43
    0b00111101,            // 'D', ord 0x44
    0b01001111,            // 'E', ord 0x45
    0b01000111,            // 'F', ord 0x46
    0b01011110,            // 'G', ord 0x47
    0b00110111,            // 'H', ord 0x48
    0b00110000,            // 'I', ord 0x49
    0b00111100,            // 'J', ord 0x4A
    MAX7219_UNKNOWN_CHAR,  // 'K', ord 0x4B
    0b00001110,            // 'L', ord 0x4C
    MAX7219_UNKNOWN_CHAR,  // 'M', ord 0x4D
    0b00010101,            // 'N', ord 0x4E
    0b01111110,            // 'O', ord 0x4F
    0b01100111,            // 'P', ord 0x50
    0b11111110,            // 'Q', ord 0x51
    0b00000101,            // 'R', ord 0x52
    0b01011011,            // 'S', ord 0x53
    0b00000111,            // 'T', ord 0x54
    0b00111110,            // 'U', ord 0x55
    0b00111110,            // 'V', ord 0x56
    0b00111111,            // 'W', ord 0x57
    MAX7219_UNKNOWN_CHAR,  // 'X', ord 0x58
    0b00100111,            // 'Y', ord 0x59
    0b01101101,            // 'Z', ord 0x5A
    0b01001110,            // '[', ord 0x5B
    MAX7219_UNKNOWN_CHAR,  // '\', ord 0x5C
    0b01111000,            // ']', ord 0x5D
    MAX7219_UNKNOWN_CHAR,  // '^', ord 0x5E
    0b00001000,            // '_', ord 0x5F
    0b00100000,            // '`', ord 0x60
    0b01110111,            // 'a', ord 0x61
    0b00011111,            // 'b', ord 0x62
    0b00001101,            // 'c', ord 0x63
    0b00111101,            // 'd', ord 0x64
    0b01001111,            // 'e', ord 0x65
    0b01000111,            // 'f', ord 0x66
    0b01011110,            // 'g', ord 0x67
    0b00010111,            // 'h', ord 0x68
    0b00010000,            // 'i', ord 0x69
    0b00111100,            // 'j', ord 0x6A
    MAX7219_UNKNOWN_CHAR,  // 'k', ord 0x6B
    0b00001110,            // 'l', ord 0x6C
    MAX7219_UNKNOWN_CHAR,  // 'm', ord 0x6D
    0b00010101,            // 'n', ord 0x6E
    0b00011101,            // 'o', ord 0x6F
    0b01100111,            // 'p', ord 0x70
    MAX7219_UNKNOWN_CHAR,  // 'q', ord 0x71
    0b00000101,            // 'r', ord 0x72
    0b01011011,            // 's', ord 0x73
    0b00000111,            // 't', ord 0x74
    0b00011100,            // 'u', ord 0x75
    0b00011100,            // 'v', ord 0x76
    MAX7219_UNKNOWN_CHAR,  // 'w', ord 0x77
    MAX7219_UNKNOWN_CHAR,  // 'x', ord 0x78
    0b00100111,            // 'y', ord 0x79
    MAX7219_UNKNOWN_CHAR,  // 'z', ord 0x7A
    0b00110001,            // '{', ord 0x7B
    0b00000110,            // '|', ord 0x7C
    0b00000111,            // '}', ord 0x7D
    0b01100011,            // '~', ord 0x7E (degree symbol)
};

float MAX7219Component::get_setup_priority() const { return setup_priority::PROCESSOR; }
void MAX7219Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MAX7219...");
  this->spi_setup();
  this->buffer_ = new uint8_t[this->num_chips_ * 8];  // NOLINT
  for (uint8_t i = 0; i < this->num_chips_ * 8; i++)
    this->buffer_[i] = 0;

  // let's assume the user has all 8 digits connected, only important in daisy chained setups anyway
  this->send_to_all_(MAX7219_REGISTER_SCAN_LIMIT, 7);
  // let's use our own ASCII -> led pattern encoding
  this->send_to_all_(MAX7219_REGISTER_DECODE_MODE, 0);
  this->send_to_all_(MAX7219_REGISTER_INTENSITY, this->intensity_);
  this->display();
  // power up
  this->send_to_all_(MAX7219_REGISTER_TEST, 0);
  this->send_to_all_(MAX7219_REGISTER_SHUTDOWN, 1);
}
void MAX7219Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX7219:");
  ESP_LOGCONFIG(TAG, "  Number of Chips: %u", this->num_chips_);
  ESP_LOGCONFIG(TAG, "  Intensity: %u", this->intensity_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

void MAX7219Component::display() {
  for (uint8_t i = 0; i < 8; i++) {
    this->enable();
    for (uint8_t j = 0; j < this->num_chips_; j++) {
      if (reverse_) {
        this->send_byte_(8 - i, buffer_[(num_chips_ - j - 1) * 8 + i]);
      } else {
        this->send_byte_(8 - i, buffer_[j * 8 + i]);
      }
    }
    this->disable();
  }
}
void MAX7219Component::send_byte_(uint8_t a_register, uint8_t data) {
  this->write_byte(a_register);
  this->write_byte(data);
}
void MAX7219Component::send_to_all_(uint8_t a_register, uint8_t data) {
  this->enable();
  for (uint8_t i = 0; i < this->num_chips_; i++)
    this->send_byte_(a_register, data);
  this->disable();
}
void MAX7219Component::update() {
  if (this->intensity_changed_) {
    this->send_to_all_(MAX7219_REGISTER_INTENSITY, this->intensity_);
    this->intensity_changed_ = false;
  }
  for (uint8_t i = 0; i < this->num_chips_ * 8; i++)
    this->buffer_[i] = 0;
  if (this->writer_.has_value())
    (*this->writer_)(*this);
  this->display();
}
uint8_t MAX7219Component::print(uint8_t start_pos, const char *str) {
  uint8_t pos = start_pos;
  for (; *str != '\0'; str++) {
    uint8_t data = MAX7219_UNKNOWN_CHAR;
    if (*str >= ' ' && *str <= '~')
      data = progmem_read_byte(&MAX7219_ASCII_TO_RAW[*str - ' ']);

    if (data == MAX7219_UNKNOWN_CHAR) {
      ESP_LOGW(TAG, "Encountered character '%c' with no MAX7219 representation while translating string!", *str);
    }
    if (*str == '.') {
      if (pos != start_pos)
        pos--;
      this->buffer_[pos] |= 0b10000000;
    } else {
      if (pos >= this->num_chips_ * 8) {
        ESP_LOGE(TAG, "MAX7219 String is too long for the display!");
        break;
      }
      this->buffer_[pos] = data;
    }
    pos++;
  }
  return pos - start_pos;
}
uint8_t MAX7219Component::print(const char *str) { return this->print(0, str); }
uint8_t MAX7219Component::printf(uint8_t pos, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(pos, buffer);
  return 0;
}
uint8_t MAX7219Component::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(buffer);
  return 0;
}
void MAX7219Component::set_writer(max7219_writer_t &&writer) { this->writer_ = writer; }
void MAX7219Component::set_intensity(uint8_t intensity) {
  intensity &= 0xF;
  if (intensity != this->intensity_) {
    this->intensity_changed_ = true;
    this->intensity_ = intensity;
  }
}
void MAX7219Component::set_num_chips(uint8_t num_chips) { this->num_chips_ = num_chips; }

uint8_t MAX7219Component::strftime(uint8_t pos, const char *format, ESPTime time) {
  char buffer[64];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    return this->print(pos, buffer);
  return 0;
}
uint8_t MAX7219Component::strftime(const char *format, ESPTime time) { return this->strftime(0, format, time); }

}  // namespace max7219
}  // namespace esphome
