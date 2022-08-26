#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include <cstdarg>

#ifdef USE_ARDUINO
#include <HardwareSerial.h>
#endif
#ifdef USE_ESP_IDF
#include <driver/uart.h>
#endif

namespace esphome {

namespace logger {

/** Enum for logging UART selection
 *
 * Advanced configuration (pin selection, etc) is not supported.
 */
enum UARTSelection {
  UART_SELECTION_UART0 = 0,
  UART_SELECTION_UART1,
#if defined(USE_ESP32)
#if !defined(USE_ESP32_VARIANT_ESP32C3) && !defined(USE_ESP32_VARIANT_ESP32S2) && !defined(USE_ESP32_VARIANT_ESP32S3)
  UART_SELECTION_UART2,
#endif
#ifdef USE_ESP_IDF
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
  UART_SELECTION_USB_CDC,
#endif
#if defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32S3)
  UART_SELECTION_USB_SERIAL_JTAG,
#endif
#endif
#endif
#ifdef USE_ESP8266
  UART_SELECTION_UART0_SWAP,
#endif
};

class Logger : public Component {
 public:
  explicit Logger(uint32_t baud_rate, size_t tx_buffer_size, UARTSelection uart);

  /// Manually set the baud rate for serial, set to 0 to disable.
  void set_baud_rate(uint32_t baud_rate);
  uint32_t get_baud_rate() const { return baud_rate_; }
#ifdef USE_ARDUINO
  Stream *get_hw_serial() const { return hw_serial_; }
#endif
#ifdef USE_ESP_IDF
  uart_port_t get_uart_num() const { return uart_num_; }
#endif

  /// Get the UART used by the logger.
  UARTSelection get_uart() const;

  /// Set the log level of the specified tag.
  void set_log_level(const std::string &tag, int log_level);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Set up this component.
  void pre_setup();
  void dump_config() override;

  int level_for(const char *tag);

  /// Register a callback that will be called for every log message sent
  void add_on_log_callback(std::function<void(int, const char *, const char *)> &&callback);

  float get_setup_priority() const override;

  void log_vprintf_(int level, const char *tag, int line, const char *format, va_list args);  // NOLINT
#ifdef USE_STORE_LOG_STR_IN_FLASH
  void log_vprintf_(int level, const char *tag, int line, const __FlashStringHelper *format, va_list args);  // NOLINT
#endif

 protected:
  void write_header_(int level, const char *tag, int line);
  void write_footer_();
  void log_message_(int level, const char *tag, int offset = 0);

  inline bool is_buffer_full_() const { return this->tx_buffer_at_ >= this->tx_buffer_size_; }
  inline int buffer_remaining_capacity_() const { return this->tx_buffer_size_ - this->tx_buffer_at_; }
  inline void reset_buffer_() { this->tx_buffer_at_ = 0; }
  inline void set_null_terminator_() {
    // does not increment buffer_at
    this->tx_buffer_[this->tx_buffer_at_] = '\0';
  }
  inline void write_to_buffer_(char value) {
    if (!this->is_buffer_full_())
      this->tx_buffer_[this->tx_buffer_at_++] = value;
  }
  inline void write_to_buffer_(const char *value, int length) {
    for (int i = 0; i < length && !this->is_buffer_full_(); i++) {
      this->tx_buffer_[this->tx_buffer_at_++] = value[i];
    }
  }
  inline void vprintf_to_buffer_(const char *format, va_list args) {
    if (this->is_buffer_full_())
      return;
    int remaining = this->buffer_remaining_capacity_();
    int ret = vsnprintf(this->tx_buffer_ + this->tx_buffer_at_, remaining, format, args);
    if (ret < 0) {
      // Encoding error, do not increment buffer_at
      return;
    }
    if (ret >= remaining) {
      // output was too long, truncated
      ret = remaining;
    }
    this->tx_buffer_at_ += ret;
  }
  inline void printf_to_buffer_(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    this->vprintf_to_buffer_(format, arg);
    va_end(arg);
  }

  uint32_t baud_rate_;
  char *tx_buffer_{nullptr};
  int tx_buffer_at_{0};
  int tx_buffer_size_{0};
  UARTSelection uart_{UART_SELECTION_UART0};
#ifdef USE_ARDUINO
  Stream *hw_serial_{nullptr};
#endif
#ifdef USE_ESP_IDF
  uart_port_t uart_num_;
#endif
  struct LogLevelOverride {
    std::string tag;
    int level;
  };
  std::vector<LogLevelOverride> log_levels_;
  CallbackManager<void(int, const char *, const char *)> log_callback_{};
  /// Prevents recursive log calls, if true a log message is already being processed.
  bool recursion_guard_ = false;
};

extern Logger *global_logger;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class LoggerMessageTrigger : public Trigger<int, const char *, const char *> {
 public:
  explicit LoggerMessageTrigger(Logger *parent, int level) {
    this->level_ = level;
    parent->add_on_log_callback([this](int level, const char *tag, const char *message) {
      if (level <= this->level_) {
        this->trigger(level, tag, message);
      }
    });
  }

 protected:
  int level_;
};

}  // namespace logger

}  // namespace esphome
