#include "logger.h"

#ifdef ARDUINO_ARCH_ESP32
#include <esp_log.h>
#endif
#include <HardwareSerial.h>

namespace esphome {
namespace logger {

static const char *TAG = "logger";

int HOT Logger::log_vprintf_(int level, const char *tag, const char *format, va_list args) {  // NOLINT
  if (level > this->level_for(tag))
    return 0;

  int ret = vsnprintf(this->tx_buffer_.data(), this->tx_buffer_.capacity(), format, args);
  this->log_message_(level, tag, this->tx_buffer_.data(), ret);
  return ret;
}
#ifdef USE_STORE_LOG_STR_IN_FLASH
int Logger::log_vprintf_(int level, const char *tag, const __FlashStringHelper *format, va_list args) {  // NOLINT
  if (level > this->level_for(tag))
    return 0;

  // copy format string
  const char *format_pgm_p = (PGM_P) format;
  size_t len = 0;
  char *write = this->tx_buffer_.data();
  char ch = '.';
  while (len < this->tx_buffer_.capacity() && ch != '\0') {
    *write++ = ch = pgm_read_byte(format_pgm_p++);
    len++;
  }
  if (len == this->tx_buffer_.capacity())
    return -1;

  // now apply vsnprintf
  size_t offset = len + 1;
  size_t remaining = this->tx_buffer_.capacity() - offset;
  char *msg = this->tx_buffer_.data() + offset;
  int ret = vsnprintf(msg, remaining, this->tx_buffer_.data(), args);
  this->log_message_(level, tag, msg, ret);
  return ret;
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
  return this->global_log_level_;
}
void HOT Logger::log_message_(int level, const char *tag, char *msg, int ret) {
  if (ret <= 0)
    return;
  // remove trailing newline
  if (msg[ret - 1] == '\n') {
    msg[ret - 1] = '\0';
  }
  if (this->baud_rate_ > 0)
    this->hw_serial_->println(msg);
  this->log_callback_.call(level, tag, msg);
}

Logger::Logger(uint32_t baud_rate, size_t tx_buffer_size, UARTSelection uart) : baud_rate_(baud_rate), uart_(uart) {
  this->set_tx_buffer_size(tx_buffer_size);
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
    this->hw_serial_->setDebugOutput(this->global_log_level_ >= ESPHOME_LOG_LEVEL_VERBOSE);
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
  if (this->global_log_level_ >= ESPHOME_LOG_LEVEL_VERBOSE) {
    esp_log_level_set("*", ESP_LOG_VERBOSE);
  }
#endif

  ESP_LOGI(TAG, "Log initialized");
}
void Logger::set_baud_rate(uint32_t baud_rate) { this->baud_rate_ = baud_rate; }
void Logger::set_global_log_level(int log_level) { this->global_log_level_ = log_level; }
void Logger::set_log_level(const std::string &tag, int log_level) {
  this->log_levels_.push_back(LogLevelOverride{tag, log_level});
}
void Logger::set_tx_buffer_size(size_t tx_buffer_size) { this->tx_buffer_.reserve(tx_buffer_size); }
UARTSelection Logger::get_uart() const { return this->uart_; }
void Logger::add_on_log_callback(std::function<void(int, const char *, const char *)> &&callback) {
  this->log_callback_.add(std::move(callback));
}
float Logger::get_setup_priority() const { return setup_priority::HARDWARE - 1.0f; }
const char *LOG_LEVELS[] = {"NONE", "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE", "VERY_VERBOSE"};
#ifdef ARDUINO_ARCH_ESP32
const char *UART_SELECTIONS[] = {"UART0", "UART1", "UART2"};
#endif
#ifdef ARDUINO_ARCH_ESP8266
const char *UART_SELECTIONS[] = {"UART0", "UART1", "UART0_SWAP"};
#endif
void Logger::dump_config() {
  ESP_LOGCONFIG(TAG, "Logger:");
  ESP_LOGCONFIG(TAG, "  Level: %s", LOG_LEVELS[this->global_log_level_]);
  ESP_LOGCONFIG(TAG, "  Log Baud Rate: %u", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Hardware UART: %s", UART_SELECTIONS[this->uart_]);
  for (auto &it : this->log_levels_) {
    ESP_LOGCONFIG(TAG, "  Level for '%s': %s", it.tag.c_str(), LOG_LEVELS[it.level]);
  }
}

Logger *global_logger = nullptr;

}  // namespace logger
}  // namespace esphome
