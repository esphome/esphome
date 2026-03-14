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
// Override esp_log_format to prevent V2's 3-call vprintf fragmentation.
// Without this, Log V2 calls the vprintf hook 3 times per message (header,
// body, newline) which creates 3 separate log entries in ESPHome's logger.
// This strong definition overrides the archive symbol from ESP-IDF's liblog,
// affecting all callers including precompiled blobs (e.g. wifi).
#include <esp_private/log_message.h>
#include <esp_log_write.h>

// Outlined cold path for early boot and constrained environment logging.
// Used before ESPHome's hook is installed (early boot) and for constrained
// env calls (PHY init, efuse reads) where the ESPHome hook would crash
// due to fwrite lock issues on USB JTAG devices.
// Formats messages in ESPHome style with ANSI colors into a 512-byte stack
// buffer, then outputs atomically via esp_rom_printf.
// Not IRAM_ATTR — flash is accessible in all cases this is called (cache
// is enabled; constrained_env is set because the scheduler isn't running
// or PHY init context, not because flash cache is disabled).
static void __attribute__((noinline)) esp_log_format_early_(esp_log_msg_t *message) {
  // ESP-IDF levels: NONE=0 ERROR=1 WARN=2 INFO=3 DEBUG=4 VERBOSE=5
  // Color digits: E=1(red) W=3(yellow) I=2(green) D=6(cyan) V=7(gray)
  static const char color_digit[] = {'\0', '1', '3', '2', '6', '7'};
  static const char lvl[] = {'\0', 'E', 'W', 'I', 'D', 'V'};
  // Format into stack buffer and output atomically via esp_rom_printf.
  // Can't use fwrite (locks crash during early boot and PHY init).
  char buf[512];
  int pos = 0;
  uint8_t level = message->config.opts.log_level;
  if (level > 0 && level < sizeof(lvl)) {
    pos = snprintf(buf, sizeof(buf), "\033[0;3%cm[%c][%s]: ", color_digit[level], lvl[level],
                   message->tag ? message->tag : "idf");
  }
  if (pos >= 0 && pos < (int) sizeof(buf) - 2) {
    int body = vsnprintf(buf + pos, sizeof(buf) - pos, message->format, message->args);
    if (body > 0)
      pos += (body < (int) sizeof(buf) - pos) ? body : (int) sizeof(buf) - pos - 1;
  }
  if (level > 0 && level < sizeof(lvl) && pos < (int) sizeof(buf) - 6) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "\033[0m");
  }
  if (pos < (int) sizeof(buf) - 1) {
    buf[pos++] = '\n';
  }
  buf[pos] = '\0';
  esp_rom_printf("%s", buf);
}

extern "C" {
// Override esp_log_format from liblog.a to prevent V2's 3-call vprintf
// fragmentation. IRAM_ATTR to match ESP-IDF's linker fragment placement.
void IRAM_ATTR esp_log_format(esp_log_msg_t *message) {
  extern vprintf_like_t esp_log_vprint_func;
  extern int vprintf(const char *, __gnuc_va_list);  // NOLINT
  if (esp_log_vprint_func == &vprintf || message->config.opts.constrained_env) [[unlikely]] {
    // Early boot or constrained env (PHY init, ISR): can't use the ESPHome
    // hook (fwrite locks crash during PHY init on USB JTAG devices).
    // Format to stack buffer with vsnprintf + esp_rom_printf (both safe
    // without stdio locks). vsnprintf writes to a buffer (no stdio init
    // needed), esp_rom_printf is ROM-resident.
    esp_log_format_early_(message);
    return;
  }
  // After hook installed, normal environment: skip formatting, forward body only.
  // Call esp_log_vprint_func directly to avoid pulling in esp_rom_vprintf
  // (1.2KB IRAM) through the esp_log_vprintf inline.
  esp_log_vprint_func(message->format, message->args);
}
}  // extern "C"
#endif
