#include "sevenseg.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace sevenseg {

static const char *const TAG = "sevenseg";

static const uint8_t SEVENSEG_UNKNOWN_CHAR = 0b00000000;

const uint8_t SEVENSEG_ASCII_TO_RAW[128] PROGMEM = {
    SEVENSEG_UNKNOWN_CHAR,  // 0x00
    SEVENSEG_UNKNOWN_CHAR,  // 0x01
    SEVENSEG_UNKNOWN_CHAR,  // 0x02
    SEVENSEG_UNKNOWN_CHAR,  // 0x03
    SEVENSEG_UNKNOWN_CHAR,  // 0x04
    SEVENSEG_UNKNOWN_CHAR,  // 0x05
    SEVENSEG_UNKNOWN_CHAR,  // 0x06
    SEVENSEG_UNKNOWN_CHAR,  // 0x07
    SEVENSEG_UNKNOWN_CHAR,  // 0x08
    SEVENSEG_UNKNOWN_CHAR,  // 0x09
    SEVENSEG_UNKNOWN_CHAR,  // 0x0A
    SEVENSEG_UNKNOWN_CHAR,  // 0x0B
    SEVENSEG_UNKNOWN_CHAR,  // 0x0C
    SEVENSEG_UNKNOWN_CHAR,  // 0x0D
    SEVENSEG_UNKNOWN_CHAR,  // 0x0E
    SEVENSEG_UNKNOWN_CHAR,  // 0x0F
    SEVENSEG_UNKNOWN_CHAR,  // 0x10
    SEVENSEG_UNKNOWN_CHAR,  // 0x11
    SEVENSEG_UNKNOWN_CHAR,  // 0x12
    SEVENSEG_UNKNOWN_CHAR,  // 0x13
    SEVENSEG_UNKNOWN_CHAR,  // 0x14
    SEVENSEG_UNKNOWN_CHAR,  // 0x15
    SEVENSEG_UNKNOWN_CHAR,  // 0x16
    SEVENSEG_UNKNOWN_CHAR,  // 0x17
    SEVENSEG_UNKNOWN_CHAR,  // 0x18
    SEVENSEG_UNKNOWN_CHAR,  // 0x19
    SEVENSEG_UNKNOWN_CHAR,  // 0x1A
    SEVENSEG_UNKNOWN_CHAR,  // 0x1B
    SEVENSEG_UNKNOWN_CHAR,  // 0x1C
    SEVENSEG_UNKNOWN_CHAR,  // 0x1D
    SEVENSEG_UNKNOWN_CHAR,  // 0x1E
    SEVENSEG_UNKNOWN_CHAR,  // 0x1F
    0b00000000,             // ' ', ord 0x20
    0b10110000,             // '!', ord 0x21
    0b00100010,             // '"', ord 0x22
    SEVENSEG_UNKNOWN_CHAR,  // '#', ord 0x23
    SEVENSEG_UNKNOWN_CHAR,  // '$', ord 0x24
    0b01001001,             // '%', ord 0x25
    SEVENSEG_UNKNOWN_CHAR,  // '&', ord 0x26
    0b00000010,             // ''', ord 0x27
    0b01001110,             // '(', ord 0x28
    0b01111000,             // ')', ord 0x29
    0b01000000,             // '*', ord 0x2A
    SEVENSEG_UNKNOWN_CHAR,  // '+', ord 0x2B
    0b00010000,             // ',', ord 0x2C
    0b00000001,             // '-', ord 0x2D
    0b10000000,             // '.', ord 0x2E
    SEVENSEG_UNKNOWN_CHAR,  // '/', ord 0x2F
    0b01111110,             // '0', ord 0x30
    0b00110000,             // '1', ord 0x31
    0b01101101,             // '2', ord 0x32
    0b01111001,             // '3', ord 0x33
    0b00110011,             // '4', ord 0x34
    0b01011011,             // '5', ord 0x35
    0b01011111,             // '6', ord 0x36
    0b01110000,             // '7', ord 0x37
    0b01111111,             // '8', ord 0x38
    0b01111011,             // '9', ord 0x39
    0b01001000,             // ':', ord 0x3A
    0b01011000,             // ';', ord 0x3B
    SEVENSEG_UNKNOWN_CHAR,  // '<', ord 0x3C
    0b00001001,             // '=', ord 0x3D
    SEVENSEG_UNKNOWN_CHAR,  // '>', ord 0x3E
    0b01100101,             // '?', ord 0x3F
    0b01101111,             // '@', ord 0x40
    0b01110111,             // 'A', ord 0x41
    0b00011111,             // 'B', ord 0x42
    0b01001110,             // 'C', ord 0x43
    0b00111101,             // 'D', ord 0x44
    0b01001111,             // 'E', ord 0x45
    0b01000111,             // 'F', ord 0x46
    0b01011110,             // 'G', ord 0x47
    0b00110111,             // 'H', ord 0x48
    0b00110000,             // 'I', ord 0x49
    0b00111100,             // 'J', ord 0x4A
    SEVENSEG_UNKNOWN_CHAR,  // 'K', ord 0x4B
    0b00001110,             // 'L', ord 0x4C
    SEVENSEG_UNKNOWN_CHAR,  // 'M', ord 0x4D
    0b00010101,             // 'N', ord 0x4E
    0b01111110,             // 'O', ord 0x4F
    0b01100111,             // 'P', ord 0x50
    0b11111110,             // 'Q', ord 0x51
    0b00000101,             // 'R', ord 0x52
    0b01011011,             // 'S', ord 0x53
    0b00000111,             // 'T', ord 0x54
    0b00111110,             // 'U', ord 0x55
    0b00111110,             // 'V', ord 0x56
    0b00111111,             // 'W', ord 0x57
    SEVENSEG_UNKNOWN_CHAR,  // 'X', ord 0x58
    0b00100111,             // 'Y', ord 0x59
    0b01101101,             // 'Z', ord 0x5A
    0b01001110,             // '[', ord 0x5B
    SEVENSEG_UNKNOWN_CHAR,  // '\', ord 0x5C
    0b01111000,             // ']', ord 0x5D
    SEVENSEG_UNKNOWN_CHAR,  // '^', ord 0x5E
    0b00001000,             // '_', ord 0x5F
    0b00100000,             // '`', ord 0x60
    0b01110111,             // 'a', ord 0x61
    0b00011111,             // 'b', ord 0x62
    0b00001101,             // 'c', ord 0x63
    0b00111101,             // 'd', ord 0x64
    0b01001111,             // 'e', ord 0x65
    0b01000111,             // 'f', ord 0x66
    0b01011110,             // 'g', ord 0x67
    0b00010111,             // 'h', ord 0x68
    0b00010000,             // 'i', ord 0x69
    0b00111100,             // 'j', ord 0x6A
    SEVENSEG_UNKNOWN_CHAR,  // 'k', ord 0x6B
    0b00001110,             // 'l', ord 0x6C
    SEVENSEG_UNKNOWN_CHAR,  // 'm', ord 0x6D
    0b00010101,             // 'n', ord 0x6E
    0b00011101,             // 'o', ord 0x6F
    0b01100111,             // 'p', ord 0x70
    SEVENSEG_UNKNOWN_CHAR,  // 'q', ord 0x71
    0b00000101,             // 'r', ord 0x72
    0b01011011,             // 's', ord 0x73
    0b00000111,             // 't', ord 0x74
    0b00011100,             // 'u', ord 0x75
    0b00011100,             // 'v', ord 0x76
    SEVENSEG_UNKNOWN_CHAR,  // 'w', ord 0x77
    SEVENSEG_UNKNOWN_CHAR,  // 'x', ord 0x78
    0b00100111,             // 'y', ord 0x79
    SEVENSEG_UNKNOWN_CHAR,  // 'z', ord 0x7A
    0b00110001,             // '{', ord 0x7B
    0b00000110,             // '|', ord 0x7C
    0b00000111,             // '}', ord 0x7D
    0b01100011,             // '~', ord 0x7E (degree symbol)
    SEVENSEG_UNKNOWN_CHAR,  // 0x7F
};

// getter and setter
float SEVENSEGComponent::get_setup_priority() const { return setup_priority::PROCESSOR; }
void SEVENSEGComponent::set_writer(sevenseg_writer_t &&writer) { this->writer_ = writer; }
void SEVENSEGComponent::set_a_pin(GPIOPin *a_pin) { this->a_pin_ = a_pin; }
void SEVENSEGComponent::set_b_pin(GPIOPin *b_pin) { this->b_pin_ = b_pin; }
void SEVENSEGComponent::set_c_pin(GPIOPin *c_pin) { this->c_pin_ = c_pin; }
void SEVENSEGComponent::set_d_pin(GPIOPin *d_pin) { this->d_pin_ = d_pin; }
void SEVENSEGComponent::set_e_pin(GPIOPin *e_pin) { this->e_pin_ = e_pin; }
void SEVENSEGComponent::set_f_pin(GPIOPin *f_pin) { this->f_pin_ = f_pin; }
void SEVENSEGComponent::set_g_pin(GPIOPin *g_pin) { this->g_pin_ = g_pin; }
void SEVENSEGComponent::set_dp_pin(GPIOPin *dp_pin) { this->dp_pin_ = dp_pin; }
void SEVENSEGComponent::set_hold_time(uint16_t hold_time) { this->hold_time_ = hold_time; }
void SEVENSEGComponent::set_blank_time(uint16_t blank_time) { this->blank_time_ = blank_time; }
void SEVENSEGComponent::set_digits(const std::vector<GPIOPin *> &digits) { this->digits_ = digits; }

// setup
void SEVENSEGComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up 7 Segment...");
  // ckeck all pins are defined
  if (!this->a_pin_ || !this->b_pin_ || !this->c_pin_ || !this->d_pin_ || !this->e_pin_ || !this->f_pin_ ||
      !this->g_pin_ || !this->dp_pin_) {
    ESP_LOGE(TAG, "Not all pins are defined.");
    return;
  }
  this->a_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->a_pin_->setup();
  this->a_pin_->digital_write(false);
  this->b_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->b_pin_->setup();
  this->b_pin_->digital_write(false);
  this->c_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->c_pin_->setup();
  this->c_pin_->digital_write(false);
  this->d_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->d_pin_->setup();
  this->d_pin_->digital_write(false);
  this->e_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->e_pin_->setup();
  this->e_pin_->digital_write(false);
  this->f_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->f_pin_->setup();
  this->f_pin_->digital_write(false);
  this->g_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->g_pin_->setup();
  this->g_pin_->digital_write(false);
  this->dp_pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->dp_pin_->setup();
  this->dp_pin_->digital_write(false);
  uint8_t ct = 0;
  for (GPIOPin *pin : this->digits_) {
    pin->pin_mode(gpio::FLAG_OUTPUT);
    pin->setup();
    pin->digital_write(false);
    ct++;
  }
  this->num_digits_ = ct;
  this->buffer_ = new uint8_t[this->num_digits_];
  this->setup_complete_ = true;
}

// dump config
void SEVENSEGComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SEVENSEG:");
  ESP_LOGCONFIG(TAG, "A Pin: %s", this->a_pin_->dump_summary().c_str());
  ESP_LOGCONFIG(TAG, "B Pin: %s", this->b_pin_->dump_summary().c_str());
  ESP_LOGCONFIG(TAG, "C Pin: %s", this->c_pin_->dump_summary().c_str());
  ESP_LOGCONFIG(TAG, "D Pin: %s", this->d_pin_->dump_summary().c_str());
  ESP_LOGCONFIG(TAG, "E Pin: %s", this->e_pin_->dump_summary().c_str());
  ESP_LOGCONFIG(TAG, "F Pin: %s", this->f_pin_->dump_summary().c_str());
  ESP_LOGCONFIG(TAG, "G Pin: %s", this->g_pin_->dump_summary().c_str());
  ESP_LOGCONFIG(TAG, "DP Pin: %s", this->dp_pin_->dump_summary().c_str());
  uint8_t ct = 0;
  for (GPIOPin *pin : this->digits_) {
    ESP_LOGCONFIG(TAG, "Digit %u Pin: %s", ct, pin->dump_summary().c_str());
    ct++;
  }
  ESP_LOGCONFIG(TAG, "Number of Digits: %u", this->num_digits_);
  ESP_LOGCONFIG(TAG, "Hold Time: %u", this->hold_time_);
  ESP_LOGCONFIG(TAG, "Blank Time: %u", this->blank_time_);
  ESP_LOGCONFIG(TAG, "update interval: %u", this->update_interval_);
  ESP_LOGCONFIG(TAG, "Writer: %s", this->writer_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "Setup Complete: %s", this->setup_complete_ ? "YES" : "NO");
  // ESP_LOGCONFIG(TAG, "  Number of Digits: %u", this->num_chips_);
  LOG_UPDATE_INTERVAL(this);
}

// update
void SEVENSEGComponent::update() {
  if (this->writer_.has_value())
    (*this->writer_)(*this);
  this->display();
}

// display
void SEVENSEGComponent::display() {
  for (uint8_t i = 0; i < this->num_digits_; i++) {
    this->set_digit_(i, this->buffer_[i], false);
  }
}

// void SEVENSEGComponent::update() { this->display(); }

// blank display
void SEVENSEGComponent::clear_display_() {
  this->a_pin_->digital_write(false);
  this->b_pin_->digital_write(false);
  this->c_pin_->digital_write(false);
  this->d_pin_->digital_write(false);
  this->e_pin_->digital_write(false);
  this->f_pin_->digital_write(false);
  this->g_pin_->digital_write(false);
  this->dp_pin_->digital_write(false);
  delay(this->blank_time_);
}

// write digit to segment
void SEVENSEGComponent::set_digit_(uint8_t digit, uint8_t ch, bool dot) {
  uint8_t segments = 0;
  // concat to printable ASCII characters
  if (ch < 128) {
    segments = SEVENSEG_ASCII_TO_RAW[ch];
  } else {
    segments = 128;
  }
  segments = SEVENSEG_ASCII_TO_RAW[ch];

  // write binary representation of the segments
  this->clear_display_();

  uint8_t ct = 0;
  for (GPIOPin *pin : this->digits_) {
    pin->digital_write(ct == digit);
    ct++;
  }

  this->dp_pin_->digital_write((segments & 0b10000000) || dot);
  this->a_pin_->digital_write(segments & 0b01000000);
  this->b_pin_->digital_write(segments & 0b00100000);
  this->c_pin_->digital_write(segments & 0b00010000);
  this->d_pin_->digital_write(segments & 0b00001000);
  this->e_pin_->digital_write(segments & 0b00000100);
  this->f_pin_->digital_write(segments & 0b00000010);
  this->g_pin_->digital_write(segments & 0b00000001);
  delay(this->hold_time_);
};

// print functions
uint8_t SEVENSEGComponent::print(uint8_t start_pos, const char *str) {
  uint8_t pos = start_pos;
  for (; *str != '\0'; str++) {
    uint8_t data = SEVENSEG_UNKNOWN_CHAR;
    if (*str >= ' ' && *str <= '~')
      data = progmem_read_byte(&SEVENSEG_ASCII_TO_RAW[(unsigned char) *str]);

    if (*str == '.') {
      if (pos != start_pos)
        pos--;
      this->buffer_[pos] |= 0b10000000;
    } else {
      if (pos >= this->num_digits_) {
        ESP_LOGE(TAG, "String is too long for the display!");
        break;
      }
      this->buffer_[pos] = data;
    }
    pos++;
  }
  return pos - start_pos;
}

uint8_t SEVENSEGComponent::print(const char *str) { return this->print(0, str); }

uint8_t SEVENSEGComponent::print(std::string str) { return this->print(0, str.c_str()); }

uint8_t SEVENSEGComponent::printf(uint8_t pos, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(pos, buffer);
  return 0;
}

uint8_t SEVENSEGComponent::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[64];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    return this->print(buffer);
  return 0;
}

uint8_t SEVENSEGComponent::strftime(uint8_t pos, const char *format, ESPTime time) {
  char buffer[64];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    return this->print(pos, buffer);
  return 0;
}

uint8_t SEVENSEGComponent::strftime(const char *format, ESPTime time) { return this->strftime(0, format, time); }

}  // namespace sevenseg
}  // namespace esphome
