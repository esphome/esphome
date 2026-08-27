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

// Only compiled when the logger component is present: the override exists to
// integrate V2 with ESPHome's logger hook. Without the logger no hook is ever
// installed, so liblog's stock esp_log_format links in and console output
// stays bone-stock ESP-IDF (V2's flash savings still apply).
#if defined(USE_ESP32_LOG_V2) && defined(USE_LOGGER) && !defined(BOOTLOADER_BUILD)
// Override esp_log_format to prevent V2's 3-call vprintf fragmentation.
// Without this, Log V2 calls the vprintf hook 3 times per message (header,
// body, newline) which creates 3 separate log entries in ESPHome's logger.
// Interception uses the linker's --wrap (added in esp32/__init__.py when Log
// V2 is enabled): a plain strong definition cannot win here because liblog's
// log.c.obj references esp_log_format and the linker resolves it from
// log_format_text.c.obj within the same archive, before ever reaching
// ESPHome's archive, which then collides as a duplicate definition.
#include <esp_private/log_message.h>
#include <esp_log_write.h>

// Format an ESP-IDF log message directly to the console, bypassing the
// ESPHome logger hook. Used when the hook isn't installed (early boot) or
// can't be used safely (constrained env: PHY init, efuse reads -- fwrite
// locks crash on USB JTAG devices).
// Formats in ESPHome style with ANSI colors into a 512-byte stack buffer,
// then outputs atomically via esp_rom_printf.
// This path is cold on 99.9% of builds -- it only runs during early boot
// and at DEBUG framework log level (default is ERROR).
static void __attribute__((noinline)) esp_log_format_direct_(esp_log_msg_t *message) {  // NOLINT
  // ESP-IDF levels: NONE=0 ERROR=1 WARN=2 INFO=3 DEBUG=4 VERBOSE=5
  // Color digits: E=1(red) W=3(yellow) I=2(green) D=6(cyan) V=7(gray)
  static const char COLOR_DIGIT[] = {'\0', '1', '3', '2', '6', '7'};
  static const char LVL[] = {'\0', 'E', 'W', 'I', 'D', 'V'};
  // Format into stack buffer and output atomically via esp_rom_printf.
  // Can't use fwrite (locks crash during early boot and PHY init).
  char buf[512];
  int pos = 0;
  uint8_t level = message->config.opts.log_level;
  if (level > 0 && level < sizeof(LVL)) {
    pos = snprintf(buf, sizeof(buf), "\033[0;3%cm[%c][%s]: ", COLOR_DIGIT[level], LVL[level],
                   message->tag ? message->tag : "idf");
    // Clamp: snprintf returns the untruncated length (or negative on error),
    // so an oversized tag or an encoding error would otherwise leave pos
    // outside the buffer and break every bound check below.
    if (pos < 0) {
      pos = 0;
    } else if (pos >= (int) sizeof(buf)) {
      pos = sizeof(buf) - 1;
    }
  }
  if (pos < (int) sizeof(buf) - 2) {
    int body = vsnprintf(buf + pos, sizeof(buf) - pos, message->format, message->args);
    if (body > 0)
      pos += (body < (int) sizeof(buf) - pos) ? body : (int) sizeof(buf) - pos - 1;
  }
  if (level > 0 && level < sizeof(LVL) && pos < (int) sizeof(buf) - 6) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "\033[0m");
  }
  if (pos < (int) sizeof(buf) - 1) {
    buf[pos++] = '\n';
  }
  buf[pos] = '\0';
  esp_rom_printf("%s", buf);
}

extern "C" {
// Wrap of esp_log_format from liblog.a (via -Wl,--wrap=esp_log_format) to
// prevent V2's 3-call vprintf fragmentation. Deliberately NOT IRAM_ATTR: every
// caller that must work with flash cache disabled (ESP_DRAM_LOGx,
// ESP_EARLY_LOGx) bypasses esp_log() entirely under
// CONFIG_LOG_API_CONSTRAINED_ENV_SAFE=n, and both paths below immediately call
// flash-resident code anyway, so IRAM placement would only spend the IRAM this
// change exists to save.
void __wrap_esp_log_format(esp_log_msg_t *message) {  // NOLINT
  extern vprintf_like_t esp_log_vprint_func;
  extern int vprintf(const char *, __gnuc_va_list);  // NOLINT
  if (esp_log_vprint_func == &vprintf || message->config.opts.constrained_env) [[unlikely]] {
    // Early boot or constrained env (PHY init, efuse reads, scheduler not
    // running). Can't use the ESPHome hook -- fwrite locks crash during PHY
    // init on USB JTAG devices and newlib isn't initialized during early boot.
    // Format to stack buffer with vsnprintf + esp_rom_printf instead.
    //
    // Note: if called from an ISR with flash cache disabled, this will crash
    // because the format string and tag are in flash. This is the same as V1
    // where ESP_EARLY_LOGx from ISR also used flash-resident format strings
    // via esp_rom_printf. No ESP-IDF code is known to log from ISR with
    // cache disabled.
    esp_log_format_direct_(message);
    return;
  }
  // After hook installed, normal environment. Never call the esp_log_vprintf
  // inline here: it would pull in esp_rom_vprintf (1.2KB IRAM).
  if (esp_log_vprint_func == &esphome::esp_idf_log_vprintf_ && esphome::logger::global_logger != nullptr) {
    // The hook is ESPHome's own (the common case). V2 keeps the component tag
    // (wifi, phy_init, ...) and per-message severity separate from the format
    // string, so call the logger directly with both: lines render with the
    // real tag and level (e.g. "[E][wifi]: ...") including color and per-tag
    // filtering, details the (format, args) hook signature cannot carry.
    // IDF levels: NONE=0 E=1 W=2 I=3 D=4 V=5; ESPHome inserts CONFIG at 4.
    static const uint8_t LEVEL_MAP[] = {ESPHOME_LOG_LEVEL_NONE, ESPHOME_LOG_LEVEL_ERROR, ESPHOME_LOG_LEVEL_WARN,
                                        ESPHOME_LOG_LEVEL_INFO, ESPHOME_LOG_LEVEL_DEBUG, ESPHOME_LOG_LEVEL_VERBOSE};
    uint8_t idf_level = message->config.opts.log_level;
    uint8_t level = idf_level < sizeof(LEVEL_MAP) ? LEVEL_MAP[idf_level] : ESPHOME_LOG_LEVEL_VERBOSE;
    esphome::logger::global_logger->log_vprintf_(level, message->tag ? message->tag : "esp-idf", 0, message->format,
                                                 message->args);
    return;
  }
  // A custom hook was installed via esp_log_set_vprintf: honor it and forward
  // the message body as-is.
  esp_log_vprint_func(message->format, message->args);
}
}  // extern "C"
#endif
