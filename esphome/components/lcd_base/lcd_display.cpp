#include "lcd_display.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace lcd_base {

static const char *const TAG = "lcd";

// First set bit determines command, bits after that are the data.
static const uint8_t LCD_DISPLAY_COMMAND_CLEAR_DISPLAY = 0x01;
static const uint8_t LCD_DISPLAY_COMMAND_RETURN_HOME = 0x02;
static const uint8_t LCD_DISPLAY_COMMAND_ENTRY_MODE_SET = 0x04;
static const uint8_t LCD_DISPLAY_COMMAND_DISPLAY_CONTROL = 0x08;
static const uint8_t LCD_DISPLAY_COMMAND_CURSOR_SHIFT = 0x10;
static const uint8_t LCD_DISPLAY_COMMAND_FUNCTION_SET = 0x20;
static const uint8_t LCD_DISPLAY_COMMAND_SET_CGRAM_ADDR = 0x40;
static const uint8_t LCD_DISPLAY_COMMAND_SET_DDRAM_ADDR = 0x80;

static const uint8_t LCD_DISPLAY_ENTRY_SHIFT_INCREMENT = 0x01;
static const uint8_t LCD_DISPLAY_ENTRY_LEFT = 0x02;

static const uint8_t LCD_DISPLAY_DISPLAY_BLINK_ON = 0x01;
static const uint8_t LCD_DISPLAY_DISPLAY_CURSOR_ON = 0x02;
static const uint8_t LCD_DISPLAY_DISPLAY_ON = 0x04;

static const uint8_t LCD_DISPLAY_FUNCTION_8_BIT_MODE = 0x10;
static const uint8_t LCD_DISPLAY_FUNCTION_2_LINE = 0x08;
static const uint8_t LCD_DISPLAY_FUNCTION_5X10_DOTS = 0x04;

void LCDDisplay::setup() {
  this->buffer_ = new uint8_t[this->rows_ * this->columns_];  // NOLINT
  for (uint8_t i = 0; i < this->rows_ * this->columns_; i++)
    this->buffer_[i] = ' ';

  uint8_t display_function = 0;

  if (!this->is_four_bit_mode())
    display_function |= LCD_DISPLAY_FUNCTION_8_BIT_MODE;

  if (this->rows_ > 1)
    display_function |= LCD_DISPLAY_FUNCTION_2_LINE;

  // TODO dotsize

  // Commands can only be sent 40ms after boot-up, so let's wait if we're close
  const uint8_t now = millis();
  if (now < 40)
    delay(40u - now);

  if (this->is_four_bit_mode()) {
    this->write_n_bits(0x03, 4);
    delay(5);  // 4.1ms
    this->write_n_bits(0x03, 4);
    delay(5);
    this->write_n_bits(0x03, 4);
    delayMicroseconds(150);
    this->write_n_bits(0x02, 4);
  } else {
    this->command_(LCD_DISPLAY_COMMAND_FUNCTION_SET | display_function);
    delay(5);  // 4.1ms
    this->command_(LCD_DISPLAY_COMMAND_FUNCTION_SET | display_function);
    delayMicroseconds(150);
    this->command_(LCD_DISPLAY_COMMAND_FUNCTION_SET | display_function);
  }

  // store user defined characters
  for (auto &user_defined_char : this->user_defined_chars_) {
    this->command_(LCD_DISPLAY_COMMAND_SET_CGRAM_ADDR | (user_defined_char.first << 3));
    for (auto data : user_defined_char.second)
      this->send(data, true);
  }

  this->command_(LCD_DISPLAY_COMMAND_FUNCTION_SET | display_function);
  uint8_t display_control = LCD_DISPLAY_DISPLAY_ON;
  this->command_(LCD_DISPLAY_COMMAND_DISPLAY_CONTROL | display_control);

  // clear display, also sets DDRAM address to 0 (home)
  this->command_(LCD_DISPLAY_COMMAND_CLEAR_DISPLAY);
  delay(2);  // 1.52ms

  uint8_t entry_mode = LCD_DISPLAY_ENTRY_LEFT;
  this->command_(LCD_DISPLAY_COMMAND_ENTRY_MODE_SET | entry_mode);  // 37µs

  this->command_(LCD_DISPLAY_COMMAND_RETURN_HOME);
  delay(2);  // 1.52ms
}

float LCDDisplay::get_setup_priority() const { return setup_priority::PROCESSOR; }
void HOT LCDDisplay::display() {
  this->command_(LCD_DISPLAY_COMMAND_SET_DDRAM_ADDR | 0);

  for (uint8_t i = 0; i < this->columns_; i++)
    this->send(this->buffer_[i], true);

  if (this->rows_ >= 3) {
    for (uint8_t i = 0; i < this->columns_; i++)
      this->send(this->buffer_[this->columns_ * 2 + i], true);
  }

  if (this->rows_ >= 1) {
    this->command_(LCD_DISPLAY_COMMAND_SET_DDRAM_ADDR | 0x40);

    for (uint8_t i = 0; i < this->columns_; i++)
      this->send(this->buffer_[this->columns_ + i], true);

    if (this->rows_ >= 4) {
      for (uint8_t i = 0; i < this->columns_; i++)
        this->send(this->buffer_[this->columns_ * 3 + i], true);
    }
  }
}
void LCDDisplay::update() {
  this->clear();
  this->call_writer();
  this->display();
}
void LCDDisplay::command_(uint8_t value) { this->send(value, false); }
void LCDDisplay::print(uint8_t column, uint8_t row, const char *str) {
  uint8_t pos = column + row * this->columns_;
  for (; *str != '\0'; str++) {
    if (*str == '\n') {
      pos = ((pos / this->columns_) + 1) * this->columns_;
      continue;
    }
    if (pos >= this->rows_ * this->columns_) {
      ESP_LOGW(TAG, "LCDDisplay writing out of range!");
      break;
    }

    this->buffer_[pos] = *reinterpret_cast<const uint8_t *>(str);
    pos++;
  }
}
void LCDDisplay::print(uint8_t column, uint8_t row, const std::string &str) { this->print(column, row, str.c_str()); }
void LCDDisplay::print(const char *str) { this->print(0, 0, str); }
void LCDDisplay::print(const std::string &str) { this->print(0, 0, str.c_str()); }
void LCDDisplay::printf(uint8_t column, uint8_t row, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    this->print(column, row, buffer);
}
void LCDDisplay::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    this->print(0, 0, buffer);
}
void LCDDisplay::clear() {
  for (uint8_t i = 0; i < this->rows_ * this->columns_; i++)
    this->buffer_[i] = ' ';
}
void LCDDisplay::strftime(uint8_t column, uint8_t row, const char *format, ESPTime time) {
  char buffer[64];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    this->print(column, row, buffer);
}
void LCDDisplay::strftime(const char *format, ESPTime time) { this->strftime(0, 0, format, time); }
void LCDDisplay::loadchar(uint8_t location, uint8_t charmap[]) {
  location &= 0x7;  // we only have 8 locations 0-7
  this->command_(LCD_DISPLAY_COMMAND_SET_CGRAM_ADDR | (location << 3));
  for (int i = 0; i < 8; i++) {
    this->send(charmap[i], true);
  }
}

}  // namespace lcd_base
}  // namespace esphome
