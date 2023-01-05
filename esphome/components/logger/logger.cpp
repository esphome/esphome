#include "logger.h"

#ifdef USE_ESP_IDF
#include <driver/uart.h>
#include "freertos/FreeRTOS.h"
#endif  // USE_ESP_IDF

#if defined(USE_ESP32_FRAMEWORK_ARDUINO) || defined(USE_ESP_IDF)
#include <esp_log.h>
#endif  // USE_ESP32_FRAMEWORK_ARDUINO || USE_ESP_IDF
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

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
  this->printf_to_buffer_("%s[%s][%s:%03u]: ", color, letter, tag, line);
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
  if (this->baud_rate_ > 0) {
#ifdef USE_ARDUINO
    this->hw_serial_->println(msg);
#endif  // USE_ARDUINO
#ifdef USE_ESP_IDF
    if (
#if defined(USE_ESP32_VARIANT_ESP32S2)
        uart_ == UART_SELECTION_USB_CDC
#elif defined(USE_ESP32_VARIANT_ESP32C3)
        uart_ == UART_SELECTION_USB_SERIAL_JTAG
#elif defined(USE_ESP32_VARIANT_ESP32S3)
        uart_ == UART_SELECTION_USB_CDC || uart_ == UART_SELECTION_USB_SERIAL_JTAG
#else
        /* DISABLES CODE */ (false)
#endif
    ) {
      puts(msg);
    } else {
      uart_write_bytes(uart_num_, msg, strlen(msg));
      uart_write_bytes(uart_num_, "\n", 1);
    }
#endif
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
}

void Logger::pre_setup() {
  if (this->baud_rate_ > 0) {
#ifdef USE_ARDUINO
    switch (this->uart_) {
      case UART_SELECTION_UART0:
#ifdef USE_ESP8266
      case UART_SELECTION_UART0_SWAP:
#endif
#ifdef USE_RP2040
        this->hw_serial_ = &Serial1;
        Serial1.begin(this->baud_rate_);
#else
        this->hw_serial_ = &Serial;
        Serial.begin(this->baud_rate_);
#endif
#ifdef USE_ESP8266
        if (this->uart_ == UART_SELECTION_UART0_SWAP) {
          Serial.swap();
        }
        Serial.setDebugOutput(ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE);
#endif
        break;
      case UART_SELECTION_UART1:
#ifdef USE_RP2040
        this->hw_serial_ = &Serial2;
        Serial2.begin(this->baud_rate_);
#else
        this->hw_serial_ = &Serial1;
        Serial1.begin(this->baud_rate_);
#endif
#ifdef USE_ESP8266
        Serial1.setDebugOutput(ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE);
#endif
        break;
#if defined(USE_ESP32) && !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32S2) && \
    !defined(USE_ESP32_VARIANT_ESP32S3)
      case UART_SELECTION_UART2:
        this->hw_serial_ = &Serial2;
        Serial2.begin(this->baud_rate_);
        break;
#endif
#ifdef USE_RP2040
      case UART_SELECTION_USB_CDC:
        this->hw_serial_ = &Serial;
        Serial.begin(this->baud_rate_);
        break;
#endif
    }
#endif  // USE_ARDUINO
#ifdef USE_ESP_IDF
    uart_num_ = UART_NUM_0;
    switch (uart_) {
      case UART_SELECTION_UART0:
        uart_num_ = UART_NUM_0;
        break;
      case UART_SELECTION_UART1:
        uart_num_ = UART_NUM_1;
        break;
#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32S2) && !defined(USE_ESP32_VARIANT_ESP32S3)
      case UART_SELECTION_UART2:
        uart_num_ = UART_NUM_2;
        break;
#endif  // !USE_ESP32_VARIANT_ESP32C3 && !USE_ESP32_VARIANT_ESP32S2 && !USE_ESP32_VARIANT_ESP32S3
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
      case UART_SELECTION_USB_CDC:
        uart_num_ = -1;
        break;
#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
#if defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S3)
      case UART_SELECTION_USB_SERIAL_JTAG:
        uart_num_ = -1;
        break;
#endif  // USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32S3
    }
    if (uart_num_ >= 0) {
      uart_config_t uart_config{};
      uart_config.baud_rate = (int) baud_rate_;
      uart_config.data_bits = UART_DATA_8_BITS;
      uart_config.parity = UART_PARITY_DISABLE;
      uart_config.stop_bits = UART_STOP_BITS_1;
      uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
      uart_param_config(uart_num_, &uart_config);
      const int uart_buffer_size = tx_buffer_size_;
      // Install UART driver using an event queue here
      uart_driver_install(uart_num_, uart_buffer_size, uart_buffer_size, 10, nullptr, 0);
    }
#endif  // USE_ESP_IDF
  }
#ifdef USE_ESP8266
  else {
    uart_set_debug(UART_NO);
  }
#endif  // USE_ESP8266

  global_logger = this;
#if defined(USE_ESP_IDF) || defined(USE_ESP32_FRAMEWORK_ARDUINO)
  esp_log_set_vprintf(esp_idf_log_vprintf_);
  if (ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE) {
    esp_log_level_set("*", ESP_LOG_VERBOSE);
  }
#endif  // USE_ESP_IDF || USE_ESP32_FRAMEWORK_ARDUINO

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
float Logger::get_setup_priority() const { return setup_priority::BUS + 500.0f; }
const char *const LOG_LEVELS[] = {"NONE", "ERROR", "WARN", "INFO", "CONFIG", "DEBUG", "VERBOSE", "VERY_VERBOSE"};
#ifdef USE_ESP32
const char *const UART_SELECTIONS[] = {
    "UART0",           "UART1",
#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32S2) && !defined(USE_ESP32_VARIANT_ESP32S3)
    "UART2",
#endif  // !USE_ESP32_VARIANT_ESP32C3 && !USE_ESP32_VARIANT_ESP32S2 && !USE_ESP32_VARIANT_ESP32S3
#if defined(USE_ESP_IDF)
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
    "USB_CDC",
#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
#if defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S3)
    "USB_SERIAL_JTAG",
#endif  // USE_ESP32_VARIANT_ESP32C3 || USE_ESP32_VARIANT_ESP32S3
#endif  // USE_ESP_IDF
};
#endif  // USE_ESP32
#ifdef USE_ESP8266
const char *const UART_SELECTIONS[] = {"UART0", "UART1", "UART0_SWAP"};
#endif
#ifdef USE_RP2040
const char *const UART_SELECTIONS[] = {"UART0", "UART1", "USB_CDC"};
#endif  // USE_ESP8266
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

Logger *global_logger = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace logger
}  // namespace esphome
