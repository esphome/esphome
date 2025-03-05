#include "logger.h"
#include <cinttypes>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace logger {

static const char *const TAG = "logger";

static const char *const LOG_LEVEL_COLORS[] = {
    "",                                            // NONE
    ESPHOME_LOG_BOLD(ESPHOME_LOG_COLOR_RED),       // ERROR
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_YELLOW),   // WARNING
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_GREEN),    // INFO
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_MAGENTA),  // CONFIG
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_CYAN),     // DEBUG
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_GRAY),     // VERBOSE
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_WHITE),    // VERY_VERBOSE
};
static const char *const LOG_LEVEL_LETTERS[] = {
    "",    // NONE
    "E",   // ERROR
    "W",   // WARNING
    "I",   // INFO
    "C",   // CONFIG
    "D",   // DEBUG
    "V",   // VERBOSE
    "VV",  // VERY_VERBOSE
};

void Logger::write_header_(int level, const char *tag, int line) {
  if (level < 0)
    level = 0;
  if (level > 7)
    level = 7;

  const char *color = LOG_LEVEL_COLORS[level];
  const char *letter = LOG_LEVEL_LETTERS[level];
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
#else
  void *current_task = nullptr;
#endif
  if (current_task == main_task_) {
    this->printf_to_buffer_("%s[%s][%s:%03u]: ", color, letter, tag, line);
  } else {
    const char *thread_name = "";  // NOLINT(clang-analyzer-deadcode.DeadStores)
#if defined(USE_ESP32)
    thread_name = pcTaskGetName(current_task);
#elif defined(USE_LIBRETINY)
    thread_name = pcTaskGetTaskName(current_task);
#endif
    this->printf_to_buffer_("%s[%s][%s:%03u]%s[%s]%s: ", color, letter, tag, line,
                            ESPHOME_LOG_BOLD(ESPHOME_LOG_COLOR_RED), thread_name, color);
  }
}

void HOT Logger::log_vprintf_(int level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
  if (level > this->level_for(tag) || recursion_guard_)
    return;

  recursion_guard_ = true;
  this->reset_buffer_();
  this->write_header_(level, tag, line);
  this->vprintf_to_buffer_(format, args);
  this->write_footer_();
  this->log_message_(level, tag);
  recursion_guard_ = false;
}
#ifdef USE_STORE_LOG_STR_IN_FLASH
void Logger::log_vprintf_(int level, const char *tag, int line, const __FlashStringHelper *format,
                          va_list args) {  // NOLINT
  if (level > this->level_for(tag) || recursion_guard_)
    return;

  recursion_guard_ = true;
  this->reset_buffer_();
  // copy format string
  auto *format_pgm_p = reinterpret_cast<const uint8_t *>(format);
  size_t len = 0;
  char ch = '.';
  while (!this->is_buffer_full_() && ch != '\0') {
    this->tx_buffer_[this->tx_buffer_at_++] = ch = (char) progmem_read_byte(format_pgm_p++);
  }
  // Buffer full form copying format
  if (this->is_buffer_full_())
    return;

  // length of format string, includes null terminator
  uint32_t offset = this->tx_buffer_at_;

  // now apply vsnprintf
  this->write_header_(level, tag, line);
  this->vprintf_to_buffer_(this->tx_buffer_, args);
  this->write_footer_();
  this->log_message_(level, tag, offset);
  recursion_guard_ = false;
}
#endif

int HOT Logger::level_for(const char *tag) {
  if (this->log_levels_.count(tag) != 0)
    return this->log_levels_[tag];
  return this->current_level_;
}

void HOT Logger::log_message_(int level, const char *tag, int offset) {
  // remove trailing newline
  if (this->tx_buffer_[this->tx_buffer_at_ - 1] == '\n') {
    this->tx_buffer_at_--;
  }
  // make sure null terminator is present
  this->set_null_terminator_();

  const char *msg = this->tx_buffer_ + offset;

  if (this->baud_rate_ > 0) {
    this->write_msg_(msg);
  }

#ifdef USE_ESP32
  // Suppress network-logging if memory constrained, but still log to serial
  // ports. In some configurations (eg BLE enabled) there may be some transient
  // memory exhaustion, and trying to log when OOM can lead to a crash. Skipping
  // here usually allows the stack to recover instead.
  // See issue #1234 for analysis.
  if (xPortGetFreeHeapSize() < 2048)
    return;
#endif

  this->log_callback_.call(level, tag, msg);
}

Logger::Logger(uint32_t baud_rate, size_t tx_buffer_size) : baud_rate_(baud_rate), tx_buffer_size_(tx_buffer_size) {
  // add 1 to buffer size for null terminator
  this->tx_buffer_ = new char[this->tx_buffer_size_ + 1];  // NOLINT
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  this->main_task_ = xTaskGetCurrentTaskHandle();
#endif
}

#ifdef USE_LOGGER_USB_CDC
void Logger::loop() {
#ifdef USE_ARDUINO
  if (this->uart_ != UART_SELECTION_USB_CDC) {
    return;
  }
  static bool opened = false;
  if (opened == Serial) {
    return;
  }
  if (false == opened) {
    App.schedule_dump_config();
  }
  opened = !opened;
#endif
}
#endif

void Logger::set_baud_rate(uint32_t baud_rate) { this->baud_rate_ = baud_rate; }
void Logger::set_log_level(const std::string &tag, int log_level) { this->log_levels_[tag] = log_level; }

#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY)
UARTSelection Logger::get_uart() const { return this->uart_; }
#endif

void Logger::add_on_log_callback(std::function<void(int, const char *, const char *)> &&callback) {
  this->log_callback_.add(std::move(callback));
}
float Logger::get_setup_priority() const { return setup_priority::BUS + 500.0f; }
const char *const LOG_LEVELS[] = {"NONE", "ERROR", "WARN", "INFO", "CONFIG", "DEBUG", "VERBOSE", "VERY_VERBOSE"};

void Logger::dump_config() {
  ESP_LOGCONFIG(TAG, "Logger:");
  ESP_LOGCONFIG(TAG, "  Max Level: %s", LOG_LEVELS[ESPHOME_LOG_LEVEL]);
  ESP_LOGCONFIG(TAG, "  Initial Level: %s", LOG_LEVELS[this->current_level_]);
#ifndef USE_HOST
  ESP_LOGCONFIG(TAG, "  Log Baud Rate: %" PRIu32, this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Hardware UART: %s", get_uart_selection_());
#endif

  for (auto &it : this->log_levels_) {
    ESP_LOGCONFIG(TAG, "  Level for '%s': %s", it.first.c_str(), LOG_LEVELS[it.second]);
  }
}
void Logger::write_footer_() { this->write_to_buffer_(ESPHOME_LOG_RESET_COLOR, strlen(ESPHOME_LOG_RESET_COLOR)); }

void Logger::set_log_level(int level) {
  if (level > ESPHOME_LOG_LEVEL) {
    level = ESPHOME_LOG_LEVEL;
    ESP_LOGW(TAG, "Cannot set log level higher than pre-compiled %s", LOG_LEVELS[ESPHOME_LOG_LEVEL]);
  }
  this->current_level_ = level;
  this->level_callback_.call(level);
}

Logger *global_logger = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace logger
}  // namespace esphome
