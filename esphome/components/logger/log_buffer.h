#pragma once

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::logger {

// Maximum header size: 35 bytes fixed + 32 bytes tag + 16 bytes thread name = 83 bytes (45 byte safety margin)
static constexpr uint16_t MAX_HEADER_SIZE = 128;

// ANSI color code last digit (30-38 range, store only last digit to save RAM)
static constexpr char LOG_LEVEL_COLOR_DIGIT[] = {
    '\0',  // NONE
    '1',   // ERROR (31 = red)
    '3',   // WARNING (33 = yellow)
    '2',   // INFO (32 = green)
    '5',   // CONFIG (35 = magenta)
    '6',   // DEBUG (36 = cyan)
    '7',   // VERBOSE (37 = gray)
    '8',   // VERY_VERBOSE (38 = white)
};

static constexpr char LOG_LEVEL_LETTER_CHARS[] = {
    '\0',  // NONE
    'E',   // ERROR
    'W',   // WARNING
    'I',   // INFO
    'C',   // CONFIG
    'D',   // DEBUG
    'V',   // VERBOSE (VERY_VERBOSE uses two 'V's)
};

// Buffer wrapper for log formatting functions
struct LogBuffer {
  char *data;
  uint16_t size;
  uint16_t pos{0};
  // Replaces the null terminator with a newline for console output.
  // Must be called after notify_listeners_() since listeners need null-terminated strings.
  // Console output uses length-based writes (buf.pos), so null terminator is not needed.
  void terminate_with_newline() {
    if (this->pos < this->size) {
      this->data[this->pos++] = '\n';
    } else if (this->size > 0) {
      // Buffer was full - replace last char with newline to ensure it's visible
      this->data[this->size - 1] = '\n';
      this->pos = this->size;
    }
  }
  void HOT write_header(uint8_t level, const char *tag, int line, const char *thread_name) {
    // Early return if insufficient space - intentionally don't update pos to prevent partial writes
    if (this->pos + MAX_HEADER_SIZE > this->size)
      return;

    char *p = this->current_();

    // Write ANSI color
    this->write_ansi_color_(p, level);

    // Construct: [LEVEL][tag:line]
    *p++ = '[';
    if (level != 0) {
      if (level >= 7) {
        *p++ = 'V';  // VERY_VERBOSE = "VV"
        *p++ = 'V';
      } else {
        *p++ = LOG_LEVEL_LETTER_CHARS[level];
      }
    }
    *p++ = ']';
    *p++ = '[';

    // Copy tag
    this->copy_string_(p, tag);

    *p++ = ':';

    // Format line number without modulo operations
    if (line > 999) [[unlikely]] {
      int thousands = line / 1000;
      *p++ = '0' + thousands;
      line -= thousands * 1000;
    }
    int hundreds = line / 100;
    int remainder = line - hundreds * 100;
    int tens = remainder / 10;
    *p++ = '0' + hundreds;
    *p++ = '0' + tens;
    *p++ = '0' + (remainder - tens * 10);
    *p++ = ']';

#if defined(USE_ESP32) || defined(USE_LIBRETINY) || defined(USE_ZEPHYR) || defined(USE_HOST)
    // Write thread name with bold red color
    if (thread_name != nullptr) {
      this->write_ansi_color_(p, 1);  // Bold red for thread name
      *p++ = '[';
      this->copy_string_(p, thread_name);
      *p++ = ']';
      this->write_ansi_color_(p, level);  // Restore original color
    }
#endif

    *p++ = ':';
    *p++ = ' ';

    this->pos = p - this->data;
  }
  void HOT format_body(const char *format, va_list args) {
    this->format_vsnprintf_(format, args);
    this->finalize_();
  }
#ifdef USE_STORE_LOG_STR_IN_FLASH
  void HOT format_body_P(PGM_P format, va_list args) {
    this->format_vsnprintf_P_(format, args);
    this->finalize_();
  }
#endif
  void write_body(const char *text, uint16_t text_length) {
    this->write_(text, text_length);
    this->finalize_();
  }

 private:
  bool full_() const { return this->pos >= this->size; }
  uint16_t remaining_() const { return this->size - this->pos; }
  char *current_() { return this->data + this->pos; }
  void write_(const char *value, uint16_t length) {
    const uint16_t available = this->remaining_();
    const uint16_t copy_len = (length < available) ? length : available;
    if (copy_len > 0) {
      memcpy(this->current_(), value, copy_len);
      this->pos += copy_len;
    }
  }
  void finalize_() {
    // Write color reset sequence
    static constexpr uint16_t RESET_COLOR_LEN = sizeof(ESPHOME_LOG_RESET_COLOR) - 1;
    this->write_(ESPHOME_LOG_RESET_COLOR, RESET_COLOR_LEN);
    // Null terminate
    this->data[this->full_() ? this->size - 1 : this->pos] = '\0';
  }
  void strip_trailing_newlines_() {
    while (this->pos > 0 && this->data[this->pos - 1] == '\n')
      this->pos--;
  }
  void process_vsnprintf_result_(int ret) {
    if (ret < 0)
      return;
    const uint16_t rem = this->remaining_();
    this->pos += (ret >= rem) ? (rem - 1) : static_cast<uint16_t>(ret);
    this->strip_trailing_newlines_();
  }
  void format_vsnprintf_(const char *format, va_list args) {
    if (this->full_())
      return;
    this->process_vsnprintf_result_(vsnprintf(this->current_(), this->remaining_(), format, args));
  }
#ifdef USE_STORE_LOG_STR_IN_FLASH
  void format_vsnprintf_P_(PGM_P format, va_list args) {
    if (this->full_())
      return;
    this->process_vsnprintf_result_(vsnprintf_P(this->current_(), this->remaining_(), format, args));
  }
#endif
  // Write ANSI color escape sequence to buffer, updates pointer in place
  // Caller is responsible for ensuring buffer has sufficient space
  void write_ansi_color_(char *&p, uint8_t level) {
    if (level == 0)
      return;
    // Direct buffer fill: "\033[{bold};3{color}m" (7 bytes)
    *p++ = '\033';
    *p++ = '[';
    *p++ = (level == 1) ? '1' : '0';  // Only ERROR is bold
    *p++ = ';';
    *p++ = '3';
    *p++ = LOG_LEVEL_COLOR_DIGIT[level];
    *p++ = 'm';
  }
  // Copy string without null terminator, updates pointer in place
  // Caller is responsible for ensuring buffer has sufficient space
  void copy_string_(char *&p, const char *str) {
    const size_t len = strlen(str);
    // NOLINTNEXTLINE(bugprone-not-null-terminated-result) - intentionally no null terminator, building string piece by
    // piece
    memcpy(p, str, len);
    p += len;
  }
};

}  // namespace esphome::logger
