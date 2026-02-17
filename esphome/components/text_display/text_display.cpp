#include "text_display.h"

namespace esphome::text_display {

static const char *const TAG = "text_display";

void TextDisplay::print(const char *str) { this->print(0, 0, str); }

void TextDisplay::setup() { init_(); }

void TextDisplay::update() {
  this->clear();
  this->call_writer_();
  this->display();
}

void TextDisplay::clear() {
  for (uint8_t i = 0; i < this->rows_ * this->columns_; i++)
    this->buffer_[i] = ' ';
}

void TextDisplay::print(uint8_t column, uint8_t row, const char *str) {
  uint8_t pos = column + row * this->columns_;
  for (; *str != '\0'; str++) {
    if (*str == '\n') {
      pos = ((pos / this->columns_) + 1) * this->columns_;
      continue;
    }
    if (pos >= this->rows_ * this->columns_) {
      ESP_LOGW(TAG, "TextDisplay writing out of range!");
      break;
    }

    this->buffer_[pos] = *reinterpret_cast<const uint8_t *>(str);
    pos++;
  }
}
void TextDisplay::print(uint8_t column, uint8_t row, const std::string &str) { this->print(column, row, str.c_str()); }
void TextDisplay::print(const std::string &str) { this->print(0, 0, str.c_str()); }
void TextDisplay::printf(uint8_t column, uint8_t row, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    this->print(column, row, buffer);
}
void TextDisplay::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);
  if (ret > 0)
    this->print(0, 0, buffer);
}

void TextDisplay::init_() {
  this->buffer_ = new uint8_t[this->rows_ * this->columns_];  // NOLINT
  for (uint8_t i = 0; i < this->rows_ * this->columns_; i++)
    this->buffer_[i] = ' ';
}

}  // namespace esphome::text_display
