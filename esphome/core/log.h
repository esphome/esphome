#pragma once

#include <cassert>
#include <cstdarg>
#include <string>

#ifdef USE_STORE_LOG_STR_IN_FLASH
#include "WString.h"
#endif

#include "esphome/core/macros.h"

// Include ESP-IDF/Arduino based logging methods here so they don't undefine ours later
#if defined(USE_ESP32_FRAMEWORK_ARDUINO) || defined(USE_ESP_IDF)
#include <esp_err.h>
#include <esp_log.h>
#endif
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include <esp32-hal-log.h>
#endif

#include "esphome/core/macros.h"

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
#define ESPHOME_LOG_LEVEL ESPHOME_LOG_LEVEL_DEBUG
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
#define ESPHOME_LOG_SECRET_BEGIN "\033[5m"
#define ESPHOME_LOG_SECRET_END "\033[6m"
#define LOG_SECRET(x) ESPHOME_LOG_SECRET_BEGIN x ESPHOME_LOG_SECRET_END

#define ESPHOME_LOG_COLOR(COLOR) "\033[0;" COLOR "m"
#define ESPHOME_LOG_BOLD(COLOR) "\033[1;" COLOR "m"
#define ESPHOME_LOG_RESET_COLOR "\033[0m"

void esp_log_printf_(int level, const char *tag, int line, const char *format, ...)  // NOLINT
    __attribute__((format(printf, 4, 5)));
#ifdef USE_STORE_LOG_STR_IN_FLASH
void esp_log_printf_(int level, const char *tag, int line, const __FlashStringHelper *format, ...);
#endif
void esp_log_vprintf_(int level, const char *tag, int line, const char *format, va_list args);  // NOLINT
#ifdef USE_STORE_LOG_STR_IN_FLASH
void esp_log_vprintf_(int level, const char *tag, int line, const __FlashStringHelper *format, va_list args);
#endif
#if defined(USE_ESP32_FRAMEWORK_ARDUINO) || defined(USE_ESP_IDF)
int esp_idf_log_vprintf_(const char *format, va_list args);  // NOLINT
#endif

#ifdef USE_STORE_LOG_STR_IN_FLASH
#define ESPHOME_LOG_FORMAT(format) F(format)
#else
#define ESPHOME_LOG_FORMAT(format) format
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
#define esph_log_vv(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_VERY_VERBOSE, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_VERY_VERBOSE
#else
#define esph_log_vv(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
#define esph_log_v(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_VERBOSE, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_VERBOSE
#else
#define esph_log_v(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
#define esph_log_d(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_DEBUG, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)
#define esph_log_config(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_CONFIG, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_DEBUG
#define ESPHOME_LOG_HAS_CONFIG
#else
#define esph_log_d(tag, format, ...)
#define esph_log_config(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_INFO
#define esph_log_i(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_INFO, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_INFO
#else
#define esph_log_i(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_WARN
#define esph_log_w(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_WARN, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_WARN
#else
#define esph_log_w(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
#define esph_log_e(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_ERROR, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)

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
#define LOG_E(tag, ...) ESP_LOGE(tag, __VA__ARGS__)
#define ESP_LOGW(tag, ...) esph_log_w(tag, __VA_ARGS__)
#define LOG_W(tag, ...) ESP_LOGW(tag, __VA__ARGS__)
#define ESP_LOGI(tag, ...) esph_log_i(tag, __VA_ARGS__)
#define LOG_I(tag, ...) ESP_LOGI(tag, __VA__ARGS__)
#define ESP_LOGD(tag, ...) esph_log_d(tag, __VA_ARGS__)
#define LOG_D(tag, ...) ESP_LOGD(tag, __VA__ARGS__)
#define ESP_LOGCONFIG(tag, ...) esph_log_config(tag, __VA_ARGS__)
#define LOG_CONFIG(tag, ...) ESP_LOGCONFIG(tag, __VA__ARGS__)
#define ESP_LOGV(tag, ...) esph_log_v(tag, __VA_ARGS__)
#define LOG_V(tag, ...) ESP_LOGV(tag, __VA__ARGS__)
#define ESP_LOGVV(tag, ...) esph_log_vv(tag, __VA_ARGS__)
#define LOG_VV(tag, ...) ESP_LOGVV(tag, __VA__ARGS__)

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte) \
  ((byte) &0x80 ? '1' : '0'), ((byte) &0x40 ? '1' : '0'), ((byte) &0x20 ? '1' : '0'), ((byte) &0x10 ? '1' : '0'), \
      ((byte) &0x08 ? '1' : '0'), ((byte) &0x04 ? '1' : '0'), ((byte) &0x02 ? '1' : '0'), ((byte) &0x01 ? '1' : '0')
#define YESNO(b) ((b) ? "YES" : "NO")
#define ONOFF(b) ((b) ? "ON" : "OFF")
#define TRUEFALSE(b) ((b) ? "TRUE" : "FALSE")

// Helper class that identifies strings that may be stored in flash storage (similar to Arduino's __FlashStringHelper)
struct LogString;

#ifdef USE_STORE_LOG_STR_IN_FLASH

#include <pgmspace.h>

#if ARDUINO_VERSION_CODE >= VERSION_CODE(2, 5, 0)
#define LOG_STR_ARG(s) ((PGM_P)(s))
#else
// Pre-Arduino 2.5, we can't pass a PSTR() to printf(). Emulate support by copying the message to a
// local buffer first. String length is limited to 63 characters.
// https://github.com/esp8266/Arduino/commit/6280e98b0360f85fdac2b8f10707fffb4f6e6e31
#define LOG_STR_ARG(s) \
  ({ \
    char __buf[64]; \
    __buf[63] = '\0'; \
    strncpy_P(__buf, (PGM_P)(s), 63); \
    __buf; \
  })
#endif

#define LOG_STR(s) (reinterpret_cast<const LogString *>(PSTR(s)))
#define LOG_STR_LITERAL(s) LOG_STR_ARG(LOG_STR(s))

#else  // !USE_STORE_LOG_STR_IN_FLASH

#define LOG_STR(s) (reinterpret_cast<const LogString *>(s))
#define LOG_STR_ARG(s) (reinterpret_cast<const char *>(s))
#define LOG_STR_LITERAL(s) (s)

#endif

}  // namespace esphome
