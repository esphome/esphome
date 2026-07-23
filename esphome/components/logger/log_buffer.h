#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::logger {

// Maximum header size: 35 bytes fixed + 32 bytes tag + 16 bytes thread name = 83 bytes (45 byte safety margin)
static constexpr uint16_t MAX_HEADER_SIZE = 128;

// ANSI color code last digit (30-38 range, store only last digit to save RAM on ESP8266)
static const char LOG_LEVEL_COLOR_DIGIT[] PROGMEM = {
    '\0',  // NONE
    '1',   // ERROR (31 = red)
    '3',   // WARNING (33 = yellow)
    '2',   // INFO (32 = green)
    '5',   // CONFIG (35 = magenta)
    '6',   // DEBUG (36 = cyan)
    '7',   // VERBOSE (37 = gray)
    '8',   // VERY_VERBOSE (38 = white)
};

static const char LOG_LEVEL_LETTER_CHARS[] PROGMEM = {
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
#ifdef USE_LOGGER_CRLF_LINE_ENDINGS
    if (this->remaining_() >= 2) {
      this->data[this->pos++] = '\r';
      this->data[this->pos++] = '\n';
    } else if (this->size > 1) {
      this->data[this->size - 1] = '\n';
      this->data[this->size - 2] = '\r';
      this->pos = this->size;
    }
#else
    if (this->remaining_() >= 1) {
      this->data[this->pos++] = '\n';
    } else if (this->size > 0) {
      // Buffer was full - replace last char with newline to ensure it's visible
      this->data[this->size - 1] = '\n';
      this->pos = this->size;
    }
#endif
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
        *p++ = static_cast<char>(progmem_read_byte(reinterpret_cast<const uint8_t *>(&LOG_LEVEL_LETTER_CHARS[level])));
      }
    }
    *p++ = ']';
    *p++ = '[';

    // Copy tag
    this->copy_string_(p, tag);

    *p++ = ':';

    // Format line number using subtraction loops (no division - important for ESP8266 which lacks hardware divider)
    if (line > 999) [[unlikely]] {
      write_digit(p, line, 1000);
    }
    write_digit(p, line, 100);
    write_digit(p, line, 10);
    *p++ = '0' + line;
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
#ifdef USE_STORE_LOG_STR_IN_FLASH
#define VSNPRINTF vsnprintf_P
#define FORMAT_TYPE PGM_P
#else
#define VSNPRINTF vsnprintf
#define FORMAT_TYPE const char *
#endif

  void HOT format_body(FORMAT_TYPE format, va_list args) {
    this->format_vsnprintf_(format, args);
    this->finalize_();
  }

  void write_body(const char *text, uint16_t text_length) {
    const uint16_t available = this->remaining_();
    const uint16_t copy_len = (text_length < available) ? text_length : available;
    if (copy_len > 0) {
      memcpy(this->current_(), text, copy_len);
      this->pos += copy_len;
    }
    this->finalize_();
  }

 private:
  bool full_() const { return this->pos + ANSI_RESET_LEN >= this->size; }
  uint16_t remaining_() const { return full_() ? 0 : this->size - this->pos - ANSI_RESET_LEN; }
  char *current_() { return this->data + this->pos; }
  void finalize_() {
    this->write_ansi_reset_();
    // Null terminate
    const bool full = this->pos >= this->size;
    this->data[full ? this->size - 1 : this->pos] = '\0';
  }
  // Write ANSI reset sequence inline ("\033[0m") - avoids write_() call overhead
  static constexpr uint16_t ANSI_RESET_LEN = 4;  // "\033[0m"
  void write_ansi_reset_() {
    if (this->size - this->pos >= ANSI_RESET_LEN) {
      char *p = this->current_();
      *p++ = '\033';
      *p++ = '[';
      *p++ = '0';
      *p++ = 'm';
      this->pos += ANSI_RESET_LEN;
    }
  }
  void format_vsnprintf_(FORMAT_TYPE format, va_list args) {
    if (this->full_())
      return;
    // Reserve space for the ANSI reset sequence written by finalize_() after this returns.
    const uint16_t budget = this->remaining_();
    if (budget <= 1)
      return;  // buffer full because there is no space even for null terminator
    int vsnprintf_result = VSNPRINTF(this->current_(), budget, format, args);
    const bool error = vsnprintf_result < 0;
    if (error) {
      return;  // error in vsnprintf, don't update pos or try to process result
    }
    const bool cropped = vsnprintf_result >= budget;
    uint16_t offset = cropped ? (budget - 1) : static_cast<uint16_t>(vsnprintf_result);
    if (offset <= 0) {
      return;  // nothing to append to the buffer; only null terminator
    }
    // discard trailing LF (and a preceding CR, so a manually-included trailing "\r\n" is also stripped)
    while (offset > 0 && this->data[this->pos + offset - 1] == '\n') {
      offset--;
#ifdef USE_LOGGER_CRLF_LINE_ENDINGS
      if (offset > 0 && this->data[this->pos + offset - 1] == '\r')
        offset--;
#endif
    }
#ifdef USE_LOGGER_CRLF_LINE_ENDINGS
    // inplace expansion of LF characters into CR+LF; count_chars is capped so that
    // count_chars + count_lf_chars never exceeds `budget`, discarding trailing chars rather than overflowing the
    // buffer (this also means we never split a CR+LF pair, since we stop before consuming the LF that wouldn't fit)
    uint16_t count_lf_chars = 0;
    uint16_t count_chars = 0;
    while (count_chars < offset) {
      const uint16_t i = this->pos + count_chars;
      const bool is_lone_lf = this->data[i] == '\n' && (i == 0 || this->data[i - 1] != '\r');
      const uint16_t width_if_consumed = count_chars + count_lf_chars + 1 + (is_lone_lf ? 1 : 0);
      if (width_if_consumed > budget)
        break;  // would overflow the reserved space - stop here and discard the rest
      count_chars++;
      if (is_lone_lf)
        count_lf_chars++;
    }
    if (count_lf_chars > 0) {
      uint16_t src = this->pos + count_chars;
      uint16_t dst = src + count_lf_chars;
      while (src > this->pos) {
        const char c = this->data[--src];
        this->data[--dst] = c;
        if (c == '\n' && (src == 0 || this->data[src - 1] != '\r')) {
          this->data[--dst] = '\r';
        }
      }
    }
    offset = count_chars + count_lf_chars;
#endif
    this->pos += offset;
  }
#undef VSNPRINTF
#undef FORMAT_TYPE
  // Extract one decimal digit via subtraction (no division - important for ESP8266)
  static inline void ESPHOME_ALWAYS_INLINE write_digit(char *&p, int &value, int divisor) {
    char d = '0';
    while (value >= divisor) {
      d++;
      value -= divisor;
    }
    *p++ = d;
  }
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
    *p++ = static_cast<char>(progmem_read_byte(reinterpret_cast<const uint8_t *>(&LOG_LEVEL_COLOR_DIGIT[level])));
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
