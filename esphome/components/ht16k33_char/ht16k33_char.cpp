#include "ht16k33_char.h"

#include "esphome/components/unicode/unicode.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstdarg>
#include <cstring>

namespace esphome::ht16k33_char {

ESPHOME_LOG_TAG(TAG, "display.ht16k33_char");

// Commands; the low bits carry the argument
static constexpr uint8_t COMMAND_DISPLAY_DATA = 0x00;
static constexpr uint8_t COMMAND_SYSTEM_SETUP = 0x20;   // bit 0 runs the oscillator
static constexpr uint8_t COMMAND_DISPLAY_SETUP = 0x80;  // bit 0 turns the display on, bits 1 and 2 pick the blink rate
static constexpr uint8_t BLINK_MASK = 0x06;
static constexpr uint8_t DISPLAY_SETUP_MASK = 0x07;
static constexpr uint8_t STANDBY_FLAG = 0x80;     // kept in display_setup_, not sent
static constexpr uint8_t COMMAND_DIMMING = 0xE0;  // bits 0 to 3 are the duty cycle, 1/16 to 16/16
static constexpr uint8_t OSCILLATOR_ON = 0x01;
static constexpr uint8_t DISPLAY_ON = 0x01;
static constexpr uint8_t MAX_BLINK = 3;
static constexpr uint8_t MAX_INTENSITY = 15;

// MAX_BUFFER_LENGTH in display.py plus the terminator
static constexpr size_t FORMAT_BUFFER_SIZE = 64 + 1;

// A frame is the data address followed by the 16 bytes of display memory
static constexpr uint8_t DISPLAY_RAM_SIZE = 16;
static constexpr uint8_t FRAME_SIZE = DISPLAY_RAM_SIZE + 1;

// Adafruit 0.56" backpack: digits in bytes 0, 2, 6 and 8, colon in byte 4
static constexpr uint8_t DIGITS_PER_CHIP = 4;
static constexpr uint8_t DIGIT_RAM_OFFSET[DIGITS_PER_CHIP] PROGMEM = {0, 2, 6, 8};
static constexpr uint8_t COLON_RAM_OFFSET = 4;
static constexpr uint8_t COLON_BIT = 0x02;
static constexpr uint8_t COLON_DIGIT = 2;  // the colon sits between the second and third digit
static constexpr uint8_t DECIMAL_POINT_BIT = 0x80;

// ASCII 0x20 to 0x7E; 0 is a dark digit
//
//      A
//     ---
//  F |   | B
//     -G-
//  E |   | C
//     ---
//      D
//  bit 0 is A, bit 6 is G
static constexpr uint8_t FONT[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // space ! " # $ % & '
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,  // ( ) * + , - . /
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,  // 0 1 2 3 4 5 6 7
    0x7F, 0x6F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 8 9 : ; < = > ?
    0x00, 0x77, 0x00, 0x39, 0x00, 0x79, 0x71, 0x3D,  // @ A B C D E F G
    0x76, 0x30, 0x0E, 0x00, 0x38, 0x00, 0x37, 0x3F,  // H I J K L M N O
    0x73, 0x00, 0x00, 0x6D, 0x00, 0x3E, 0x00, 0x00,  // P Q R S T U V W
    0x00, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,  // X Y Z [ \ ] ^ _
    0x00, 0x00, 0x7C, 0x58, 0x5E, 0x00, 0x00, 0x00,  // ` a b c d e f g
    0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5C,  // h i j k l m n o
    0x00, 0x00, 0x50, 0x00, 0x78, 0x1C, 0x00, 0x00,  // p q r s t u v w
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,        // x y z { | } ~
};

void HT16K33CharDisplay::setup() {
  // Cleared before the display turns on, so a message from before an MCU-only reboot never shows
  if (!this->write_all_(this->system_setup_()) || !this->write_frames_(0) ||
      !this->write_all_(this->display_setup_command_()) || !this->write_all_(COMMAND_DIMMING | this->intensity_)) {
    this->mark_failed(LOG_STR(ESP_LOG_MSG_COMM_FAIL));
  }
}

void HT16K33CharDisplay::update() {
  if (this->writer_.has_value()) {
    this->clear();
    (*this->writer_)(*this);
  }
  this->status_comm_result(this->write_frames_(this->length_));
}

void HT16K33CharDisplay::dump_config() {
  ESP_LOGCONFIG(TAG,
                "HT16K33 Character Display:\n"
                "  Message buffer: %u bytes\n"
                "  Intensity: %u",
                this->capacity_, this->intensity_);
  LOG_I2C_DEVICE(this);
#ifdef HT16K33_CHAR_SECONDARY_COUNT
  for (uint8_t address : this->secondary_addresses_) {
    ESP_LOGCONFIG(TAG, "  Secondary address: 0x%02X", address);
  }
#endif
  LOG_UPDATE_INTERVAL(this);
}

uint8_t HT16K33CharDisplay::system_setup_() const {
  return COMMAND_SYSTEM_SETUP | ((this->display_setup_ & STANDBY_FLAG) ? 0 : OSCILLATOR_ON);
}

uint8_t HT16K33CharDisplay::display_setup_command_() const {
  return COMMAND_DISPLAY_SETUP | (this->display_setup_ & DISPLAY_SETUP_MASK);
}

bool HT16K33CharDisplay::write_all_(uint8_t command) {
  bool ok = true;
  this->for_each_chip_([&](uint8_t address) {
    if (this->bus_->write(address, &command, 1) != i2c::ERROR_OK)
      ok = false;
  });
  return ok;
}

void HT16K33CharDisplay::send_(uint8_t command) {
  if (this->is_ready())
    this->status_comm_result(this->write_all_(command));
}

uint8_t HT16K33CharDisplay::find_segments_(uint32_t codepoint) const {
#ifdef USE_HT16K33_CHAR_GLYPH_OVERRIDES
  // The define is global; a display without add_characters has no table
  if (this->glyph_overrides_ != nullptr) {
    for (const uint32_t *entry = this->glyph_overrides_; *entry != 0; entry++) {
      if ((*entry & 0xFFFF) == codepoint)
        return *entry >> 16;
    }
  }
#endif
  if (codepoint < ' ' || codepoint > '~')
    return 0;
  return progmem_read_byte(&FONT[codepoint - ' ']);
}

// A decimal point lights on the digit before it and a colon only in its place; neither takes a digit
bool HT16K33CharDisplay::write_frames_(uint8_t length) {
  uint8_t pos = 0;
  bool ok = true;
  this->for_each_chip_([&](uint8_t address) {
    uint8_t frame[FRAME_SIZE] = {COMMAND_DISPLAY_DATA};
    uint8_t *ram = frame + 1;
    uint8_t digit = 0;
    while (pos < length) {
      size_t consumed;
      uint32_t codepoint = unicode::extract_unicode_codepoint(this->message_ + pos, &consumed);
      if (consumed == 0) {
        consumed = 1;
        codepoint = 0;
      }
      if (codepoint == '.') {
        if (digit > 0)
          ram[progmem_read_byte(&DIGIT_RAM_OFFSET[digit - 1])] |= DECIMAL_POINT_BIT;
      } else if (codepoint == ':') {
        if (digit == COLON_DIGIT)
          ram[COLON_RAM_OFFSET] |= COLON_BIT;
      } else {
        if (digit == DIGITS_PER_CHIP)
          break;  // the next chip takes this character
        ram[progmem_read_byte(&DIGIT_RAM_OFFSET[digit++])] = this->find_segments_(codepoint);
      }
      pos += consumed;
    }
    if (this->bus_->write(address, frame, FRAME_SIZE) != i2c::ERROR_OK)
      ok = false;
  });
  return ok;
}

uint8_t HT16K33CharDisplay::print(uint8_t pos, const char *str) {
  if (pos >= this->capacity_)
    return 0;
  if (pos > this->length_)
    memset(this->message_ + this->length_, ' ', pos - this->length_);
  uint8_t len = std::min<size_t>(strlen(str), this->capacity_ - pos);
  // Do not cut a multi-byte character; continuation bytes are 10xxxxxx
  while (len > 0 && (str[len] & 0xC0) == 0x80)
    len--;
  memcpy(this->message_ + pos, str, len);
  this->length_ = std::max<uint8_t>(this->length_, pos + len);
  this->message_[this->length_] = '\0';
  return len;
}

uint8_t HT16K33CharDisplay::vprintf_(uint8_t pos, const char *format, va_list arg) {
  char buffer[FORMAT_BUFFER_SIZE];
  if (vsnprintf(buffer, sizeof(buffer), format, arg) <= 0)
    return 0;
  return this->print(pos, buffer);
}

uint8_t HT16K33CharDisplay::printf(uint8_t pos, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  const uint8_t written = this->vprintf_(pos, format, arg);
  va_end(arg);
  return written;
}

uint8_t HT16K33CharDisplay::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  const uint8_t written = this->vprintf_(0, format, arg);
  va_end(arg);
  return written;
}

uint8_t HT16K33CharDisplay::strftime(uint8_t pos, const char *format, ESPTime time) {
  char buffer[FORMAT_BUFFER_SIZE];
  if (time.strftime(buffer, sizeof(buffer), format) == 0)
    return 0;
  return this->print(pos, buffer);
}

uint8_t HT16K33CharDisplay::clock_display(uint8_t pos, bool show_leading_zero, bool use_ampm, ESPTime time) {
  char buffer[6];
  if (time.strftime(buffer, sizeof(buffer), use_ampm ? "%I:%M" : "%H:%M") == 0)
    return 0;
  if (!show_leading_zero && buffer[0] == '0')
    buffer[0] = ' ';
  return this->print(pos, buffer);
}

void HT16K33CharDisplay::set_intensity(uint8_t intensity) {
  this->intensity_ = std::min(intensity, MAX_INTENSITY);
  this->send_(COMMAND_DIMMING | this->intensity_);
}

void HT16K33CharDisplay::set_blink(uint8_t blink) {
  if (blink > MAX_BLINK)
    blink = 0;
  this->display_setup_ = (this->display_setup_ & ~BLINK_MASK) | (blink << 1);
  this->send_(this->display_setup_command_());
}

void HT16K33CharDisplay::display_off(bool turn_off) {
  assign_bits(this->display_setup_, DISPLAY_ON, !turn_off);
  this->send_(this->display_setup_command_());
}

void HT16K33CharDisplay::display_standby(bool standby) {
  assign_bits(this->display_setup_, STANDBY_FLAG, standby);
  this->send_(this->system_setup_());
}

void HT16K33CharDisplay::blank() {
  if (this->is_ready())
    this->status_comm_result(this->write_frames_(0));
}

}  // namespace esphome::ht16k33_char
