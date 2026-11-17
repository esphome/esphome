#pragma once

#include "log_const_en.h"

#include <cassert>
#include <cstdarg>

// Debug assert that only fires when ESPHOME_DEBUG is defined (e.g. in CI/test builds).
// Zero cost in production firmware.
#ifdef ESPHOME_DEBUG
#define ESPHOME_DEBUG_ASSERT(expr) assert(expr)  // NOLINT
#else
#define ESPHOME_DEBUG_ASSERT(expr) ((void) 0)
#endif
// for PRIu32 and friends
#include <cinttypes>
#include <string>

#ifdef USE_ESP8266
#include "WString.h"
#endif

// Include ESP-IDF/Arduino based logging methods here so they don't undefine ours later
#if defined(USE_ESP32)
#include <esp_err.h>
#include <esp_log.h>
#endif
#ifdef USE_LIBRETINY
#include <lt_logger.h>
#endif

namespace esphome {

#define ESPHOME_LOG_LEVEL_NONE 0
#define ESPHOME_LOG_LEVEL_ERROR 1
#define ESPHOME_LOG_LEVEL_WARN 2
#define ESPHOME_LOG_LEVEL_INFO 3
#define ESPHOME_LOG_LEVEL_CONFIG 4
#define ESPHOME_LOG_LEVEL_DEBUG 5
#define ESPHOME_LOG_LEVEL_VERBOSE 6
#define ESPHOME_LOG_LEVEL_VERY_VERBOSE 7

#ifndef ESPHOME_LOG_LEVEL
#define ESPHOME_LOG_LEVEL ESPHOME_LOG_LEVEL_NONE
#endif

#define ESPHOME_LOG_COLOR_BLACK "30"
#define ESPHOME_LOG_COLOR_RED "31"     // ERROR
#define ESPHOME_LOG_COLOR_GREEN "32"   // INFO
#define ESPHOME_LOG_COLOR_YELLOW "33"  // WARNING
#define ESPHOME_LOG_COLOR_BLUE "34"
#define ESPHOME_LOG_COLOR_MAGENTA "35"  // CONFIG
#define ESPHOME_LOG_COLOR_CYAN "36"     // DEBUG
#define ESPHOME_LOG_COLOR_GRAY "37"     // VERBOSE
#define ESPHOME_LOG_COLOR_WHITE "38"
#define ESPHOME_LOG_SECRET_BEGIN "\033[8m"
#define ESPHOME_LOG_SECRET_END "\033[28m"
#define LOG_SECRET(x) ESPHOME_LOG_SECRET_BEGIN x ESPHOME_LOG_SECRET_END

#define ESPHOME_LOG_COLOR(COLOR) "\033[0;" COLOR "m"
#define ESPHOME_LOG_BOLD(COLOR) "\033[1;" COLOR "m"
#define ESPHOME_LOG_RESET_COLOR "\033[0m"

void esp_log_printf_(int level, const char *tag, int line, const char *format, ...)  // NOLINT
    __attribute__((format(printf, 4, 5)));
#ifdef USE_ESP8266
// NOLINTNEXTLINE(readability-identifier-naming)
void esp_log_printf_(int level, const char *tag, int line, const __FlashStringHelper *format, ...);
#endif
// esp_log_printf_ for a line with no arguments and no '%': it is copied without formatting
void esp_log_str_(int level, const char *tag, int line, const char *format);  // NOLINT
#ifdef USE_ESP8266
// NOLINTNEXTLINE(readability-identifier-naming)
void esp_log_str_(int level, const char *tag, int line, const __FlashStringHelper *format);
#endif
// Never defined or called: the ESP_LOGx macros name it in dead code so a line without arguments has its format checked
int esp_log_format_check_(const char *format, ...) __attribute__((format(printf, 1, 2)));       // NOLINT
void esp_log_vprintf_(int level, const char *tag, int line, const char *format, va_list args);  // NOLINT
#if defined(USE_ESP32)
int esp_idf_log_vprintf_(const char *format, va_list args);  // NOLINT
#endif

#ifdef USE_ESP8266
#define ESPHOME_LOG_FORMAT(format) F(format)
#else
#define ESPHOME_LOG_FORMAT(format) format
#endif

#define ESPHOME_LOG_ARGS_(level, tag, format, ...) \
  ::esphome::esp_log_printf_(level, tag, __LINE__, ESPHOME_LOG_FORMAT(format), __VA_ARGS__)
// The format check is a statement-level if (false), not sizeof: GCC 16 skips format checks in unevaluated or folded
// expressions. __builtin_strchr folds on a literal, so only a line with '%' (such as "100%%") is formatted.
#ifdef USE_ESP8266
// F() only takes a literal, so using format more than once cannot repeat a side effect
#define ESPHOME_LOG_NO_ARGS_(level, tag, format) \
  ({ \
    if (false) \
      ::esphome::esp_log_format_check_(format); \
    if (__builtin_strchr(format, '%') != nullptr) \
      ::esphome::esp_log_printf_(level, tag, __LINE__, ESPHOME_LOG_FORMAT(format)); \
    else \
      ::esphome::esp_log_str_(level, tag, __LINE__, ESPHOME_LOG_FORMAT(format)); \
  })
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
// A function argument, so format is evaluated once and a temporary such as std::string().c_str() outlives the call
// NOLINTNEXTLINE(readability-identifier-naming)
inline void __attribute__((always_inline)) esp_log_no_args_(int level, const char *tag, int line, const char *format) {
  if (__builtin_strchr(format, '%') != nullptr) {
    esp_log_printf_(level, tag, line, format);
  } else {
    esp_log_str_(level, tag, line, format);
  }
}
#pragma GCC diagnostic pop
#define ESPHOME_LOG_NO_ARGS_(level, tag, format) \
  ({ \
    if (false) \
      ::esphome::esp_log_format_check_(format); \
    ::esphome::esp_log_no_args_(level, tag, __LINE__, format); \
  })
#endif
// ESPHOME_LOG_NO_ARGS_ when there are no arguments after the format, ESPHOME_LOG_ARGS_ otherwise
#define ESPHOME_LOG_PICK_(skip, name, ...) name
#define ESPHOME_LOG_(level, tag, format, ...) \
  ESPHOME_LOG_PICK_(__VA_OPT__(, ) ESPHOME_LOG_ARGS_, ESPHOME_LOG_NO_ARGS_)(level, tag, \
                                                                            format __VA_OPT__(, ) __VA_ARGS__)

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
#define esph_log_vv(tag, format, ...) ESPHOME_LOG_(ESPHOME_LOG_LEVEL_VERY_VERBOSE, tag, format, ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_VERY_VERBOSE
#else
#define esph_log_vv(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
#define esph_log_v(tag, format, ...) ESPHOME_LOG_(ESPHOME_LOG_LEVEL_VERBOSE, tag, format, ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_VERBOSE
#else
#define esph_log_v(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
#define esph_log_d(tag, format, ...) ESPHOME_LOG_(ESPHOME_LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)
#define esph_log_config(tag, format, ...) ESPHOME_LOG_(ESPHOME_LOG_LEVEL_CONFIG, tag, format, ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_DEBUG
#define ESPHOME_LOG_HAS_CONFIG
#else
#define esph_log_d(tag, format, ...)
#define esph_log_config(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_INFO
#define esph_log_i(tag, format, ...) ESPHOME_LOG_(ESPHOME_LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_INFO
#else
#define esph_log_i(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_WARN
#define esph_log_w(tag, format, ...) ESPHOME_LOG_(ESPHOME_LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_WARN
#else
#define esph_log_w(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
#define esph_log_e(tag, format, ...) ESPHOME_LOG_(ESPHOME_LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_ERROR
#else
#define esph_log_e(tag, format, ...)
#endif

#ifdef ESP_LOGE
#undef ESP_LOGE
#endif
#ifdef ESP_LOGW
#undef ESP_LOGW
#endif
#ifdef ESP_LOGI
#undef ESP_LOGI
#endif
#ifdef ESP_LOGD
#undef ESP_LOGD
#endif
#ifdef ESP_LOGV
#undef ESP_LOGV
#endif

#define ESP_LOGE(tag, ...) esph_log_e(tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) esph_log_w(tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) esph_log_i(tag, __VA_ARGS__)
#define ESP_LOGD(tag, ...) esph_log_d(tag, __VA_ARGS__)
#define ESP_LOGCONFIG(tag, ...) esph_log_config(tag, __VA_ARGS__)
#define ESP_LOGV(tag, ...) esph_log_v(tag, __VA_ARGS__)
#define ESP_LOGVV(tag, ...) esph_log_vv(tag, __VA_ARGS__)

// Log \p format with up to \p max_bytes of \p data as colon separated hex appended as its last %s argument.
// The stack buffer exists only when the level is compiled in. Needs esphome/core/helpers.h.
#define ESPHOME_LOG_HEX_(log_macro, tag, data, len, max_bytes, format, ...) \
  do { \
    char esph_log_hex_buf_[::esphome::format_hex_pretty_size(max_bytes)]; \
    log_macro(tag, format, __VA_ARGS__ __VA_OPT__(, )::esphome::format_hex_pretty_to(esph_log_hex_buf_, data, len)); \
  } while (0)
#ifdef ESPHOME_LOG_HAS_DEBUG
#define ESP_LOGD_HEX(tag, data, len, max_bytes, format, ...) \
  ESPHOME_LOG_HEX_(ESP_LOGD, tag, data, len, max_bytes, format, __VA_ARGS__)
#else
#define ESP_LOGD_HEX(tag, data, len, max_bytes, format, ...) \
  do { \
  } while (0)
#endif
#ifdef ESPHOME_LOG_HAS_VERBOSE
#define ESP_LOGV_HEX(tag, data, len, max_bytes, format, ...) \
  ESPHOME_LOG_HEX_(ESP_LOGV, tag, data, len, max_bytes, format, __VA_ARGS__)
#else
#define ESP_LOGV_HEX(tag, data, len, max_bytes, format, ...) \
  do { \
  } while (0)
#endif
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
#define ESP_LOGVV_HEX(tag, data, len, max_bytes, format, ...) \
  ESPHOME_LOG_HEX_(ESP_LOGVV, tag, data, len, max_bytes, format, __VA_ARGS__)
#else
#define ESP_LOGVV_HEX(tag, data, len, max_bytes, format, ...) \
  do { \
  } while (0)
#endif

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte) \
  ((byte) & 0x80 ? '1' : '0'), ((byte) & 0x40 ? '1' : '0'), ((byte) & 0x20 ? '1' : '0'), ((byte) & 0x10 ? '1' : '0'), \
      ((byte) & 0x08 ? '1' : '0'), ((byte) & 0x04 ? '1' : '0'), ((byte) & 0x02 ? '1' : '0'), \
      ((byte) & 0x01 ? '1' : '0')
#define YESNO(b) ((b) ? "YES" : "NO")
#define ONOFF(b) ((b) ? "ON" : "OFF")
#define TRUEFALSE(b) ((b) ? "TRUE" : "FALSE")
// for use with optional values
#define ONOFFMAYBE(b) (((b).has_value()) ? ONOFF((b).value()) : "UNKNOWN")

// Helper class that identifies strings that may be stored in flash storage (similar to Arduino's __FlashStringHelper)
struct LogString;

#ifdef USE_ESP8266

#include <pgmspace.h>

#define LOG_STR_ARG(s) ((PGM_P) (s))

#define LOG_STR(s) (reinterpret_cast<const LogString *>(PSTR(s)))
#define LOG_STR_LITERAL(s) LOG_STR_ARG(LOG_STR(s))

#else  // !USE_ESP8266

#define LOG_STR(s) (reinterpret_cast<const LogString *>(s))
#define LOG_STR_ARG(s) (reinterpret_cast<const char *>(s))
#define LOG_STR_LITERAL(s) (s)

#endif

// Declares a static log tag at namespace scope, in a source file or a header; on ESP8266 it lives in flash, so
// read it with the _P functions, never strcmp/strlen. Not usable for a class member: the ESP8266 form is a
// PROGMEM array, which cannot be initialized in a class.
#ifdef USE_ESP8266
#define ESPHOME_LOG_TAG(name, tag) static const char name[] PROGMEM = tag
#else
#define ESPHOME_LOG_TAG(name, tag) static constexpr const char *const name = tag
#endif

}  // namespace esphome
