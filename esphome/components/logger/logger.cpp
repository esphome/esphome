#include "logger.h"

#ifdef ARDUINO_ARCH_ESP32
#include <esp_log.h>
#endif
#include <HardwareSerial.h>

namespace esphome {
namespace logger {

static const char *TAG = "logger";

static const char *LOG_LEVEL_COLORS[] = {
    "",                                            // NONE
    ESPHOME_LOG_BOLD(ESPHOME_LOG_COLOR_RED),       // ERROR
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_YELLOW),   // WARNING
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_GREEN),    // INFO
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_MAGENTA),  // CONFIG
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_CYAN),     // DEBUG
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_GRAY),     // VERBOSE
    ESPHOME_LOG_COLOR(ESPHOME_LOG_COLOR_WHITE),    // VERY_VERBOSE
};
static const char *LOG_LEVEL_LETTERS[] = {
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
  this->printf_to_buffer_("%s[%s][%s:%03u]: ", color, letter, tag, line);
}

void HOT Logger::log_vprintf_(int level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
  if (level > this->level_for(tag))
    return;

  this->reset_buffer_();
  this->write_header_(level, tag, line);
  this->vprintf_to_buffer_(format, args);
  this->write_footer_();
  this->log_message_(level, tag);
}
#ifdef USE_STORE_LOG_STR_IN_FLASH
void Logger::log_vprintf_(int level, const char *tag, int line, const __FlashStringHelper *format,
                          va_list args) {  // NOLINT
  if (level > this->level_for(tag))
    return;

  this->reset_buffer_();
  // copy format string
  const char *format_pgm_p = (PGM_P) format;
  size_t len = 0;
  char ch = '.';
  while (!this->is_buffer_full_() && ch != '\0') {
    this->tx_buffer_[this->tx_buffer_at_++] = ch = pgm_read_byte(format_pgm_p++);
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
}
#endif

int HOT Logger::level_for(const char *tag) {
  // Uses std::vector<> for low memory footprint, though the vector
  // could be sorted to minimize lookup times. This feature isn't used that
  // much anyway so it doesn't matter too much.
  for (auto &it : this->log_levels_) {
    if (it.tag == tag) {
      return it.level;
    }
  }
  return ESPHOME_LOG_LEVEL;
}
void HOT Logger::log_message_(int level, const char *tag, int offset) {
  // remove trailing newline
  if (this->tx_buffer_[this->tx_buffer_at_ - 1] == '\n') {
    this->tx_buffer_at_--;
  }
  // make sure null terminator is present
  this->set_null_terminator_();

  const char *msg = this->tx_buffer_ + offset;
  if (this->baud_rate_ > 0)
    this->hw_serial_->println(msg);
#ifdef ARDUINO_ARCH_ESP32
  // Suppress network-logging if memory constrained, but still log to serial
  // ports. In some configurations (eg BLE enabled) there may be some transient
  // memory exhaustion, and trying to log when OOM can lead to a crash. Skipping
  // here usually allows the stack to recover instead.
  // See issue #1234 for analysis.
  if (xPortGetFreeHeapSize() > 2048)
    this->log_callback_.call(level, tag, msg);
#else
  this->log_callback_.call(level, tag, msg);
#endif
}

Logger::Logger(uint32_t baud_rate, size_t tx_buffer_size, UARTSelection uart)
    : baud_rate_(baud_rate), tx_buffer_size_(tx_buffer_size), uart_(uart) {
  // add 1 to buffer size for null terminator
  this->tx_buffer_ = new char[this->tx_buffer_size_ + 1];
}

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
    switch (this->uart_) {
      case UART_SELECTION_UART0:
#ifdef ARDUINO_ARCH_ESP8266
      case UART_SELECTION_UART0_SWAP:
#endif
        this->hw_serial_ = &Serial;
        break;
      case UART_SELECTION_UART1:
        this->hw_serial_ = &Serial1;
        break;
#ifdef ARDUINO_ARCH_ESP32
      case UART_SELECTION_UART2:
        this->hw_serial_ = &Serial2;
        break;
#endif
    }

    this->hw_serial_->begin(this->baud_rate_);
#ifdef ARDUINO_ARCH_ESP8266
    if (this->uart_ == UART_SELECTION_UART0_SWAP) {
      this->hw_serial_->swap();
    }
    this->hw_serial_->setDebugOutput(ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE);
#endif
  }
#ifdef ARDUINO_ARCH_ESP8266
  else {
    uart_set_debug(UART_NO);
  }
#endif

  global_logger = this;
#ifdef ARDUINO_ARCH_ESP32
  esp_log_set_vprintf(esp_idf_log_vprintf_);
  if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE) {
    esp_log_level_set("*", ESP_LOG_VERBOSE);
  }
#endif

  ESP_LOGI(TAG, "Log initialized");
}
void Logger::set_baud_rate(uint32_t baud_rate) { this->baud_rate_ = baud_rate; }
void Logger::set_log_level(const std::string &tag, int log_level) {
  this->log_levels_.push_back(LogLevelOverride{tag, log_level});
}
UARTSelection Logger::get_uart() const { return this->uart_; }
void Logger::add_on_log_callback(std::function<void(int, const char *, const char *)> &&callback) {
  this->log_callback_.add(std::move(callback));
}
float Logger::get_setup_priority() const { return setup_priority::HARDWARE - 1.0f; }
const char *LOG_LEVELS[] = {"NONE", "ERROR", "WARN", "INFO", "CONFIG", "DEBUG", "VERBOSE", "VERY_VERBOSE"};
#ifdef ARDUINO_ARCH_ESP32
const char *UART_SELECTIONS[] = {"UART0", "UART1", "UART2"};
#endif
#ifdef ARDUINO_ARCH_ESP8266
const char *UART_SELECTIONS[] = {"UART0", "UART1", "UART0_SWAP"};
#endif
void Logger::dump_config() {
  ESP_LOGCONFIG(TAG, "Logger:");
  ESP_LOGCONFIG(TAG, "  Level: %s", LOG_LEVELS[ESPHOME_LOG_LEVEL]);
  ESP_LOGCONFIG(TAG, "  Log Baud Rate: %u", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Hardware UART: %s", UART_SELECTIONS[this->uart_]);
  for (auto &it : this->log_levels_) {
    ESP_LOGCONFIG(TAG, "  Level for '%s': %s", it.tag.c_str(), LOG_LEVELS[it.level]);
  }
}
void Logger::write_footer_() { this->write_to_buffer_(ESPHOME_LOG_RESET_COLOR, strlen(ESPHOME_LOG_RESET_COLOR)); }

Logger *global_logger = nullptr;

}  // namespace logger
}  // namespace esphome
