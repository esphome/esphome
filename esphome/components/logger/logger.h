#pragma once

#include <cstdarg>
#include <map>
#ifdef USE_ESP32
#include <pthread.h>
#endif
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESPHOME_TASK_LOG_BUFFER
#include "task_log_buffer.h"
#endif

#ifdef USE_ARDUINO
#if defined(USE_ESP8266) || defined(USE_ESP32)
#include <HardwareSerial.h>
#endif  // USE_ESP8266 || USE_ESP32
#ifdef USE_RP2040
#include <HardwareSerial.h>
#include <SerialUSB.h>
#endif  // USE_RP2040
#endif  // USE_ARDUINO

#ifdef USE_ESP_IDF
#include <driver/uart.h>
#endif  // USE_ESP_IDF

namespace esphome {

namespace logger {

// Color and letter constants for log levels
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

#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY)
/** Enum for logging UART selection
 *
 * Advanced configuration (pin selection, etc) is not supported.
 */
enum UARTSelection {
#ifdef USE_LIBRETINY
  UART_SELECTION_DEFAULT = 0,
  UART_SELECTION_UART0,
#else
  UART_SELECTION_UART0 = 0,
#endif
  UART_SELECTION_UART1,
#if defined(USE_LIBRETINY) || defined(USE_ESP32_VARIANT_ESP32)
  UART_SELECTION_UART2,
#endif
#ifdef USE_LOGGER_USB_CDC
  UART_SELECTION_USB_CDC,
#endif
#ifdef USE_LOGGER_USB_SERIAL_JTAG
  UART_SELECTION_USB_SERIAL_JTAG,
#endif
#ifdef USE_ESP8266
  UART_SELECTION_UART0_SWAP,
#endif  // USE_ESP8266
};
#endif  // USE_ESP32 || USE_ESP8266 || USE_RP2040 || USE_LIBRETINY

/**
 * @brief Logger component for all ESPHome logging.
 *
 * This class implements a multi-platform logging system with protection against recursion.
 *
 * Recursion Protection Strategy:
 * - On ESP32: Uses task-specific recursion guards
 *   * Main task: Uses a dedicated boolean member variable for efficiency
 *   * Other tasks: Uses pthread TLS with a dynamically allocated key for task-specific state
 * - On other platforms: Uses a simple global recursion guard
 *
 * We use pthread TLS via pthread_key_create to create a unique key for storing
 * task-specific recursion state, which:
 * 1. Efficiently handles multiple tasks without locks or mutexes
 * 2. Works with ESP-IDF's pthread implementation that uses a linked list for TLS variables
 * 3. Avoids the limitations of the fixed FreeRTOS task local storage slots
 */
class Logger : public Component {
 public:
  explicit Logger(uint32_t baud_rate, size_t tx_buffer_size);
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  void init_log_buffer(size_t total_buffer_size);
#endif
#if defined(USE_LOGGER_USB_CDC) || defined(USE_ESP32)
  void loop() override;
#endif
  /// Manually set the baud rate for serial, set to 0 to disable.
  void set_baud_rate(uint32_t baud_rate);
  uint32_t get_baud_rate() const { return baud_rate_; }
#ifdef USE_ARDUINO
  Stream *get_hw_serial() const { return hw_serial_; }
#endif
#ifdef USE_ESP_IDF
  uart_port_t get_uart_num() const { return uart_num_; }
#endif
#ifdef USE_ESP32
  void create_pthread_key() { pthread_key_create(&log_recursion_key_, nullptr); }
#endif
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY)
  void set_uart_selection(UARTSelection uart_selection) { uart_ = uart_selection; }
  /// Get the UART used by the logger.
  UARTSelection get_uart() const;
#endif

  /// Set the default log level for this logger.
  void set_log_level(int level);
  /// Set the log level of the specified tag.
  void set_log_level(const std::string &tag, int log_level);
  int get_log_level() { return this->current_level_; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Set up this component.
  void pre_setup();
  void dump_config() override;

  inline int level_for(const char *tag);

  /// Register a callback that will be called for every log message sent
  void add_on_log_callback(std::function<void(int, const char *, const char *)> &&callback);

  // add a listener for log level changes
  void add_listener(std::function<void(int)> &&callback) { this->level_callback_.add(std::move(callback)); }

  float get_setup_priority() const override;

  void log_vprintf_(int level, const char *tag, int line, const char *format, va_list args);  // NOLINT
#ifdef USE_STORE_LOG_STR_IN_FLASH
  void log_vprintf_(int level, const char *tag, int line, const __FlashStringHelper *format, va_list args);  // NOLINT
#endif

 protected:
  void call_log_callbacks_(int level, const char *tag, const char *msg);
  void write_msg_(const char *msg);

  // Format a log message with printf-style arguments and write it to a buffer with header, footer, and null terminator
  // It's the caller's responsibility to initialize buffer_at (typically to 0)
  inline void HOT format_log_to_buffer_with_terminator_(int level, const char *tag, int line, const char *format,
                                                        va_list args, char *buffer, int *buffer_at, int buffer_size) {
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
    this->write_header_to_buffer_(level, tag, line, this->get_thread_name_(), buffer, buffer_at, buffer_size);
#else
    this->write_header_to_buffer_(level, tag, line, nullptr, buffer, buffer_at, buffer_size);
#endif
    this->format_body_to_buffer_(buffer, buffer_at, buffer_size, format, args);
    this->write_footer_to_buffer_(buffer, buffer_at, buffer_size);

    // Always ensure the buffer has a null terminator, even if we need to
    // overwrite the last character of the actual content
    if (*buffer_at >= buffer_size) {
      buffer[buffer_size - 1] = '\0';  // Truncate and ensure null termination
    } else {
      buffer[*buffer_at] = '\0';  // Normal case, append null terminator
    }
  }

  // Helper to format and send a log message to both console and callbacks
  inline void HOT log_message_to_buffer_and_send_(int level, const char *tag, int line, const char *format,
                                                  va_list args) {
    // Format to tx_buffer and prepare for output
    this->tx_buffer_at_ = 0;  // Initialize buffer position
    this->format_log_to_buffer_with_terminator_(level, tag, line, format, args, this->tx_buffer_, &this->tx_buffer_at_,
                                                this->tx_buffer_size_);

    if (this->baud_rate_ > 0) {
      this->write_msg_(this->tx_buffer_);  // If logging is enabled, write to console
    }
    this->call_log_callbacks_(level, tag, this->tx_buffer_);
  }

  // Write the body of the log message to the buffer
  inline void write_body_to_buffer_(const char *value, size_t length, char *buffer, int *buffer_at, int buffer_size) {
    // Calculate available space
    const int available = buffer_size - *buffer_at;
    if (available <= 0)
      return;

    // Determine copy length (minimum of remaining capacity and string length)
    const size_t copy_len = (length < static_cast<size_t>(available)) ? length : available;

    // Copy the data
    if (copy_len > 0) {
      memcpy(buffer + *buffer_at, value, copy_len);
      *buffer_at += copy_len;
    }
  }

  // Format string to explicit buffer with varargs
  inline void printf_to_buffer_(char *buffer, int *buffer_at, int buffer_size, const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    this->format_body_to_buffer_(buffer, buffer_at, buffer_size, format, arg);
    va_end(arg);
  }

#ifndef USE_HOST
  const char *get_uart_selection_();
#endif

  uint32_t baud_rate_;
  char *tx_buffer_{nullptr};
  int tx_buffer_at_{0};
  int tx_buffer_size_{0};
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040)
  UARTSelection uart_{UART_SELECTION_UART0};
#endif
#ifdef USE_LIBRETINY
  UARTSelection uart_{UART_SELECTION_DEFAULT};
#endif
#ifdef USE_ARDUINO
  Stream *hw_serial_{nullptr};
#endif
#ifdef USE_ESP_IDF
  uart_port_t uart_num_;
#endif
  std::map<std::string, int> log_levels_{};
  CallbackManager<void(int, const char *, const char *)> log_callback_{};
  int current_level_{ESPHOME_LOG_LEVEL_VERY_VERBOSE};
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  std::unique_ptr<logger::TaskLogBuffer> log_buffer_;  // Will be initialized with init_log_buffer
#endif
#ifdef USE_ESP32
  // Task-specific recursion guards:
  // - Main task uses a dedicated member variable for efficiency
  // - Other tasks use pthread TLS with a dynamically created key via pthread_key_create
  bool main_task_recursion_guard_{false};
  pthread_key_t log_recursion_key_;
#else
  bool global_recursion_guard_{false};  // Simple global recursion guard for single-task platforms
#endif
  CallbackManager<void(int)> level_callback_{};

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  void *main_task_ = nullptr;  // Only used for thread name identification
  const char *HOT get_thread_name_() {
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    if (current_task == main_task_) {
      return nullptr;  // Main task
    } else {
#if defined(USE_ESP32)
      return pcTaskGetName(current_task);
#elif defined(USE_LIBRETINY)
      return pcTaskGetTaskName(current_task);
#endif
    }
  }
#endif

#ifdef USE_ESP32
  inline bool HOT check_and_set_task_log_recursion_(bool is_main_task) {
    if (is_main_task) {
      const bool was_recursive = main_task_recursion_guard_;
      main_task_recursion_guard_ = true;
      return was_recursive;
    }

    intptr_t current = (intptr_t) pthread_getspecific(log_recursion_key_);
    if (current != 0)
      return true;

    pthread_setspecific(log_recursion_key_, (void *) 1);
    return false;
  }

  inline void HOT reset_task_log_recursion_(bool is_main_task) {
    if (is_main_task) {
      main_task_recursion_guard_ = false;
      return;
    }

    pthread_setspecific(log_recursion_key_, (void *) 0);
  }
#endif

  inline void HOT write_header_to_buffer_(int level, const char *tag, int line, const char *thread_name, char *buffer,
                                          int *buffer_at, int buffer_size) {
    // Format header
    if (level < 0)
      level = 0;
    if (level > 7)
      level = 7;

    const char *color = esphome::logger::LOG_LEVEL_COLORS[level];
    const char *letter = esphome::logger::LOG_LEVEL_LETTERS[level];

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
    if (thread_name != nullptr) {
      // Non-main task with thread name
      this->printf_to_buffer_(buffer, buffer_at, buffer_size, "%s[%s][%s:%03u]%s[%s]%s: ", color, letter, tag, line,
                              ESPHOME_LOG_BOLD(ESPHOME_LOG_COLOR_RED), thread_name, color);
      return;
    }
#endif
    // Main task or non ESP32/LibreTiny platform
    this->printf_to_buffer_(buffer, buffer_at, buffer_size, "%s[%s][%s:%03u]: ", color, letter, tag, line);
  }

  inline void HOT format_body_to_buffer_(char *buffer, int *buffer_at, int buffer_size, const char *format,
                                         va_list args) {
    // Get remaining capacity in the buffer
    const int remaining = buffer_size - *buffer_at;
    if (remaining <= 0)
      return;

    const int ret = vsnprintf(buffer + *buffer_at, remaining, format, args);

    if (ret < 0) {
      return;  // Encoding error, do not increment buffer_at
    }

    // Update buffer_at with the formatted length (handle truncation)
    int formatted_len = (ret >= remaining) ? remaining : ret;
    *buffer_at += formatted_len;

    // Remove all trailing newlines right after formatting
    while (*buffer_at > 0 && buffer[*buffer_at - 1] == '\n') {
      (*buffer_at)--;
    }
  }

  inline void HOT write_footer_to_buffer_(char *buffer, int *buffer_at, int buffer_size) {
    static const int RESET_COLOR_LEN = strlen(ESPHOME_LOG_RESET_COLOR);
    this->write_body_to_buffer_(ESPHOME_LOG_RESET_COLOR, RESET_COLOR_LEN, buffer, buffer_at, buffer_size);
  }
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
