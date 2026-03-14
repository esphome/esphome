#include "log.h"
#include "defines.h"
#include "helpers.h"
#include <cstdio>

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {

#ifdef ESPHOME_DEBUG
static void early_log_printf_(const char *tag, int line, const char *format, va_list args) {
  fprintf(stderr, "LOG BEFORE LOGGER INIT [%s:%d]: ", tag, line);
  vfprintf(stderr, format, args);
  fputc('\n', stderr);
  assert(false && "log called before Logger::pre_setup()");  // NOLINT
}
#endif

void HOT esp_log_printf_(int level, const char *tag, int line, const char *format, ...) {  // NOLINT
#ifdef USE_LOGGER
#ifdef ESPHOME_DEBUG
  if (logger::global_logger == nullptr) {
    va_list arg;
    va_start(arg, format);
    early_log_printf_(tag, line, format, arg);
    va_end(arg);
    return;
  }
#endif
  va_list arg;
  va_start(arg, format);
  logger::global_logger->log_vprintf_(static_cast<uint8_t>(level), tag, line, format, arg);
  va_end(arg);
#endif
}

#ifdef USE_STORE_LOG_STR_IN_FLASH
void HOT esp_log_printf_(int level, const char *tag, int line, const __FlashStringHelper *format, ...) {
#ifdef USE_LOGGER
  ESPHOME_DEBUG_ASSERT(logger::global_logger != nullptr);
  va_list arg;
  va_start(arg, format);
  logger::global_logger->log_vprintf_(static_cast<uint8_t>(level), tag, line, format, arg);
  va_end(arg);
#endif
}
#endif

void HOT esp_log_vprintf_(int level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
#ifdef USE_LOGGER
#ifdef ESPHOME_DEBUG
  if (logger::global_logger == nullptr) {
    early_log_printf_(tag, line, format, args);
    return;
  }
#endif
  logger::global_logger->log_vprintf_(static_cast<uint8_t>(level), tag, line, format, args);
#endif
}

#ifdef USE_STORE_LOG_STR_IN_FLASH
// Remove before 2026.9.0
void HOT esp_log_vprintf_(int level, const char *tag, int line, const __FlashStringHelper *format, va_list args) {
#ifdef USE_LOGGER
  ESPHOME_DEBUG_ASSERT(logger::global_logger != nullptr);
  logger::global_logger->log_vprintf_(static_cast<uint8_t>(level), tag, line, format, args);
#endif
}
#endif

#ifdef USE_ESP32
int HOT esp_idf_log_vprintf_(const char *format, va_list args) {  // NOLINT
#ifdef USE_LOGGER
#ifdef ESPHOME_DEBUG
  if (logger::global_logger == nullptr) {
    early_log_printf_("esp-idf", 0, format, args);
    return 0;
  }
#endif
  logger::global_logger->log_vprintf_(ESPHOME_LOG_LEVEL, "esp-idf", 0, format, args);
#endif
  return 0;
}
#endif

}  // namespace esphome

#if defined(USE_ESP32) && !defined(BOOTLOADER_BUILD)
// Override esp_log_format to disable ESP-IDF's own log formatting so that
// the vprintf hook receives a single call per message (just the user's format
// string + args). Without this, Log V2 makes 3 vprintf calls per message
// (header, body, newline) which fragments the output in ESPHome's logger.
// This strong definition overrides the archive symbol from ESP-IDF's liblog.
// It affects all callers including precompiled blobs (e.g. wifi).
//
// Before the ESPHome logger hook is installed (early boot), we fall through
// to the original ESP-IDF formatting so boot messages have proper formatting.
#include <esp_private/log_message.h>
#include <esp_private/log_print.h>
#include <esp_private/log_format.h>
#include <esp_log_write.h>

// Outlined cold path for early boot / constrained environment logging.
// Uses esp_log_printf/esp_log_vprintf which dispatch to esp_rom_vprintf
// for constrained environments (same as ESP-IDF's original esp_log_format).
// Must be in IRAM since it's called from the IRAM esp_log_format during
// early boot when the scheduler isn't running (constrained_env=1).
static void IRAM_ATTR __attribute__((noinline)) esp_log_format_early_(esp_log_msg_t *message) {
  // ESP-IDF levels: NONE=0 ERROR=1 WARN=2 INFO=3 DEBUG=4 VERBOSE=5
  // Color digits: E=1(red) W=3(yellow) I=2(green) D=6(cyan) V=7(gray)
  // DRAM_ATTR required: this function is IRAM_ATTR and may be called from constrained
  // environments where flash cache is disabled. All string constants must be in DRAM.
  // ESP-IDF's own log_format_text.c achieves this via linker fragment (noflash), but
  // our override is in a different compilation unit so we must use DRAM_ATTR explicitly.
  static DRAM_ATTR const char color_digit[] = {'\0', '1', '3', '2', '6', '7'};
  static DRAM_ATTR const char lvl[] = {'\0', 'E', 'W', 'I', 'D', 'V'};
  static DRAM_ATTR const char fmt_header[] = "\033[0;3%cm[%c][%s:000]: ";
  static DRAM_ATTR const char fmt_reset_nl[] = "\033[0m\n";
  static DRAM_ATTR const char fmt_nl[] = "\n";
  static DRAM_ATTR const char tag_fallback[] = "esp-idf";
  uint8_t level = message->config.opts.log_level;
#if CONFIG_LIBC_NEWLIB
  if (!message->config.opts.constrained_env) {
    flockfile(stdout);
  }
#endif
  if (level > 0 && level < sizeof(lvl)) {
    esp_log_printf(message->config, fmt_header, color_digit[level], lvl[level],
                   message->tag ? message->tag : tag_fallback);
  }
  esp_log_vprintf(message->config, message->format, message->args);
  if (level > 0 && level < sizeof(lvl)) {
    esp_log_printf(message->config, fmt_reset_nl);
  } else {
    esp_log_printf(message->config, fmt_nl);
  }
#if CONFIG_LIBC_NEWLIB
  if (!message->config.opts.constrained_env) {
    funlockfile(stdout);
  }
#endif
}

extern "C" {
// IRAM_ATTR required because ESP-IDF places esp_log_format in IRAM when
// CONFIG_LOG_IN_IRAM is enabled, and it may be called from constrained
// environments (ISR, cache disabled) where flash is inaccessible.
void IRAM_ATTR esp_log_format(esp_log_msg_t *message) {
  // Check if ESPHome's vprintf hook is installed by comparing against default.
  // Before logger init, esp_log_vprint_func == &vprintf (the default).
  extern vprintf_like_t esp_log_vprint_func;
  extern int vprintf(const char *, __gnuc_va_list);  // NOLINT
  if (esp_log_vprint_func == &vprintf || message->config.opts.constrained_env) [[unlikely]] {
    // Early boot or constrained env (ISR, cache disabled):
    // use ROM functions only — flash may be inaccessible.
    esp_log_format_early_(message);
    return;
  }
  // After hook installed, normal environment: skip formatting, forward body only
  esp_log_vprintf(message->config, message->format, message->args);
}
}  // extern "C"
#endif
