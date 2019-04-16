#pragma once

#include <cassert>
#include <cstdarg>
#include <string>
#ifdef USE_STORE_LOG_STR_IN_FLASH
#include "WString.h"
#endif

// avoid esp-idf redefining our macros
#include "esphome/core/esphal.h"

#ifdef ARDUINO_ARCH_ESP32
#include "esp_err.h"
#endif

namespace esphome {

#define ESPHOME_LOG_LEVEL_NONE 0
#define ESPHOME_LOG_LEVEL_ERROR 1
#define ESPHOME_LOG_LEVEL_WARN 2
#define ESPHOME_LOG_LEVEL_INFO 3
#define ESPHOME_LOG_LEVEL_DEBUG 4
#define ESPHOME_LOG_LEVEL_VERBOSE 5
#define ESPHOME_LOG_LEVEL_VERY_VERBOSE 6

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

#define ESPHOME_LOG_COLOR_E ESPHOME_LOG_BOLD(ESPHOME_LOG_COLOR_RED)
#define ESPHOME_LOG_COLOR_W ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_YELLOW)
#define ESPHOME_LOG_COLOR_I ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_GREEN)
#define ESPHOME_LOG_COLOR_C ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_MAGENTA)
#define ESPHOME_LOG_COLOR_D ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_CYAN)
#define ESPHOME_LOG_COLOR_V ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_GRAY)
#define ESPHOME_LOG_COLOR_VV ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_WHITE)
#define ESPHOME_LOG_RESET_COLOR "\033[0m"

int esp_log_printf_(int level, const char *tag, const char *format, ...)  // NOLINT
    __attribute__((format(printf, 3, 4)));
#ifdef USE_STORE_LOG_STR_IN_FLASH
int esp_log_printf_(int level, const char *tag, const __FlashStringHelper *format, ...);
#endif
int esp_log_vprintf_(int level, const char *tag, const char *format, va_list args);  // NOLINT
#ifdef USE_STORE_LOG_STR_IN_FLASH
int esp_log_vprintf_(int level, const char *tag, const __FlashStringHelper *format, va_list args);
#endif
int esp_idf_log_vprintf_(const char *format, va_list args);  // NOLINT

#ifdef USE_STORE_LOG_STR_IN_FLASH
#define ESPHOME_LOG_FORMAT(tag, letter, format) \
  F(ESPHOME_LOG_COLOR_##letter "[" #letter "][%s:%03u]: " format ESPHOME_LOG_RESET_COLOR), tag, __LINE__
#else
#define ESPHOME_LOG_FORMAT(tag, letter, format) \
  ESPHOME_LOG_COLOR_##letter "[" #letter "][%s:%03u]: " format ESPHOME_LOG_RESET_COLOR, tag, __LINE__
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
#define esph_log_vv(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_VERY_VERBOSE, tag, ESPHOME_LOG_FORMAT(tag, VV, format), ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_VERY_VERBOSE
#else
#define esph_log_vv(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
#define esph_log_v(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_VERBOSE, tag, ESPHOME_LOG_FORMAT(tag, V, format), ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_VERBOSE
#else
#define esph_log_v(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
#define esph_log_d(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_DEBUG, tag, ESPHOME_LOG_FORMAT(tag, D, format), ##__VA_ARGS__)

#define esph_log_config(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_DEBUG, tag, ESPHOME_LOG_FORMAT(tag, C, format), ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_DEBUG
#define ESPHOME_LOG_HAS_CONFIG
#else
#define esph_log_d(tag, format, ...)

#define esph_log_config(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_INFO
#define esph_log_i(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_INFO, tag, ESPHOME_LOG_FORMAT(tag, I, format), ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_INFO
#else
#define esph_log_i(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_WARN
#define esph_log_w(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_WARN, tag, ESPHOME_LOG_FORMAT(tag, W, format), ##__VA_ARGS__)

#define ESPHOME_LOG_HAS_WARN
#else
#define esph_log_w(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
#define esph_log_e(tag, format, ...) \
  esp_log_printf_(ESPHOME_LOG_LEVEL_ERROR, tag, ESPHOME_LOG_FORMAT(tag, E, format), ##__VA_ARGS__)

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

}  // namespace esphome
