#include "text_display.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::text_display {

ESPHOME_LOG_TAG(TAG, "text_display");

void TextDisplay::setup() {
  const size_t size = this->rows_ * this->columns_;
  RAMAllocator<uint8_t> allocator;
  this->buffer_ = allocator.allocate(size);
  if (this->buffer_ == nullptr) {
    this->mark_failed(LOG_STR("buffer allocation failed"));
    return;
  }
  this->clear();
}

void TextDisplay::update() {
  // A menu calls this directly, so the failed allocation is checked here too
  if (this->buffer_ == nullptr)
    return;
  this->clear();
  this->call_writer();
  this->display();
}

void TextDisplay::clear() {
  if (this->buffer_ == nullptr)
    return;
  memset(this->buffer_, ' ', this->rows_ * this->columns_);
}

void TextDisplay::print(uint8_t column, uint8_t row, const char *str) {
  if (this->buffer_ == nullptr)
    return;
  const uint32_t size = this->rows_ * this->columns_;
  uint32_t pos = column + row * this->columns_;
  for (; *str != '\0'; str++) {
    if (*str == '\n') {
      // 32 bits wide, so a run of newlines cannot wrap the position back into range
      pos = ((pos / this->columns_) + 1) * this->columns_;
      continue;
    }
    if (pos >= size) {
      ESP_LOGW(TAG, "TextDisplay writing out of range!");
      break;
    }

    this->buffer_[pos] = *reinterpret_cast<const uint8_t *>(str);
    pos++;
  }
}
void TextDisplay::print(uint8_t column, uint8_t row, const std::string &str) { this->print(column, row, str.c_str()); }
void TextDisplay::print(const char *str) { this->print(0, 0, str); }
void TextDisplay::print(const std::string &str) { this->print(str.c_str()); }

void TextDisplay::vprintf_(uint8_t column, uint8_t row, const char *format, va_list arg) {
  char buffer[256];
  int ret = vsnprintf(buffer, sizeof(buffer), format, arg);
  if (ret > 0)
    this->print(column, row, buffer);
}
void TextDisplay::printf(uint8_t column, uint8_t row, const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(column, row, format, arg);
  va_end(arg);
}
void TextDisplay::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  this->vprintf_(0, 0, format, arg);
  va_end(arg);
}

void TextDisplay::strftime(uint8_t column, uint8_t row, const char *format, ESPTime time) {
  char buffer[64];
  size_t ret = time.strftime(buffer, sizeof(buffer), format);
  if (ret > 0)
    this->print(column, row, buffer);
}
void TextDisplay::strftime(const char *format, ESPTime time) { this->strftime(0, 0, format, time); }

}  // namespace esphome::text_display
