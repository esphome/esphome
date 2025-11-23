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
#if defined(USE_ESP8266)
#include <HardwareSerial.h>
#endif  // USE_ESP8266
#ifdef USE_RP2040
#include <HardwareSerial.h>
#include <SerialUSB.h>
#endif  // USE_RP2040
#endif  // USE_ARDUINO

#ifdef USE_ESP32
#include <driver/uart.h>
#endif  // USE_ESP32

#ifdef USE_ZEPHYR
#include <zephyr/kernel.h>
struct device;
#endif

namespace esphome::logger {

#ifdef USE_LOGGER_RUNTIME_TAG_LEVELS
// Comparison function for const char* keys in log_levels_ map
struct CStrCompare {
  bool operator()(const char *a, const char *b) const { return strcmp(a, b) < 0; }
};
#endif

// ANSI color code last digit (30-38 range, store only last digit to save RAM)
static constexpr char LOG_LEVEL_COLOR_DIGIT[] = {
    '\0',  // NONE
    '1',   // ERROR (31 = red)
    '3',   // WARNING (33 = yellow)
    '2',   // INFO (32 = green)
    '5',   // CONFIG (35 = magenta)
    '6',   // DEBUG (36 = cyan)
    '7',   // VERBOSE (37 = gray)
    '8',   // VERY_VERBOSE (38 = white)
};

static constexpr char LOG_LEVEL_LETTER_CHARS[] = {
    '\0',  // NONE
    'E',   // ERROR
    'W',   // WARNING
    'I',   // INFO
    'C',   // CONFIG
    'D',   // DEBUG
    'V',   // VERBOSE (VERY_VERBOSE uses two 'V's)
};

// Maximum header size: 35 bytes fixed + 32 bytes tag + 16 bytes thread name = 83 bytes (45 byte safety margin)
static constexpr uint16_t MAX_HEADER_SIZE = 128;

// "0x" + 2 hex digits per byte + '\0'
static constexpr size_t MAX_POINTER_REPRESENTATION = 2 + sizeof(void *) * 2 + 1;

#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY) || defined(USE_ZEPHYR)
/** Enum for logging UART selection
 *
 * Advanced configuration (pin selection, etc) is not supported.
 */
enum UARTSelection : uint8_t {
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
#endif  // USE_ESP32 || USE_ESP8266 || USE_RP2040 || USE_LIBRETINY || USE_ZEPHYR

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
#if defined(USE_ESPHOME_TASK_LOG_BUFFER) || (defined(USE_ZEPHYR) && defined(USE_LOGGER_USB_CDC))
  void loop() override;
#endif
  /// Manually set the baud rate for serial, set to 0 to disable.
  void set_baud_rate(uint32_t baud_rate);
  uint32_t get_baud_rate() const { return baud_rate_; }
#if defined(USE_ARDUINO) && !defined(USE_ESP32)
  Stream *get_hw_serial() const { return hw_serial_; }
#endif
#ifdef USE_ESP32
  uart_port_t get_uart_num() const { return uart_num_; }
  void create_pthread_key() { pthread_key_create(&log_recursion_key_, nullptr); }
#endif
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY) || defined(USE_ZEPHYR)
  void set_uart_selection(UARTSelection uart_selection) { uart_ = uart_selection; }
  /// Get the UART used by the logger.
  UARTSelection get_uart() const;
#endif

  /// Set the default log level for this logger.
  void set_log_level(uint8_t level);
#ifdef USE_LOGGER_RUNTIME_TAG_LEVELS
  /// Set the log level of the specified tag.
  void set_log_level(const char *tag, uint8_t log_level);
#endif
  uint8_t get_log_level() { return this->current_level_; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Set up this component.
  void pre_setup();
  void dump_config() override;

  inline uint8_t level_for(const char *tag);

  /// Register a callback that will be called for every log message sent
  void add_on_log_callback(std::function<void(uint8_t, const char *, const char *, size_t)> &&callback);

  // add a listener for log level changes
  void add_listener(std::function<void(uint8_t)> &&callback) { this->level_callback_.add(std::move(callback)); }

  float get_setup_priority() const override;

  void log_vprintf_(uint8_t level, const char *tag, int line, const char *format, va_list args);  // NOLINT
#ifdef USE_STORE_LOG_STR_IN_FLASH
  void log_vprintf_(uint8_t level, const char *tag, int line, const __FlashStringHelper *format,
                    va_list args);  // NOLINT
#endif

 protected:
  void process_messages_();
  void write_msg_(const char *msg);

  // Format a log message with printf-style arguments and write it to a buffer with header, footer, and null terminator
  // It's the caller's responsibility to initialize buffer_at (typically to 0)
  inline void HOT format_log_to_buffer_with_terminator_(uint8_t level, const char *tag, int line, const char *format,
                                                        va_list args, char *buffer, uint16_t *buffer_at,
                                                        uint16_t buffer_size) {
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
    this->write_header_to_buffer_(level, tag, line, this->get_thread_name_(), buffer, buffer_at, buffer_size);
#elif defined(USE_ZEPHYR)
    char buff[MAX_POINTER_REPRESENTATION];
    this->write_header_to_buffer_(level, tag, line, this->get_thread_name_(buff), buffer, buffer_at, buffer_size);
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
  inline void HOT log_message_to_buffer_and_send_(uint8_t level, const char *tag, int line, const char *format,
                                                  va_list args) {
    // Format to tx_buffer and prepare for output
    this->tx_buffer_at_ = 0;  // Initialize buffer position
    this->format_log_to_buffer_with_terminator_(level, tag, line, format, args, this->tx_buffer_, &this->tx_buffer_at_,
                                                this->tx_buffer_size_);

    if (this->baud_rate_ > 0) {
      this->write_msg_(this->tx_buffer_);  // If logging is enabled, write to console
    }
    this->log_callback_.call(level, tag, this->tx_buffer_, this->tx_buffer_at_);
  }

  // Write the body of the log message to the buffer
  inline void write_body_to_buffer_(const char *value, size_t length, char *buffer, uint16_t *buffer_at,
                                    uint16_t buffer_size) {
    // Calculate available space
    if (*buffer_at >= buffer_size)
      return;
    const uint16_t available = buffer_size - *buffer_at;

    // Determine copy length (minimum of remaining capacity and string length)
    const size_t copy_len = (length < static_cast<size_t>(available)) ? length : available;

    // Copy the data
    if (copy_len > 0) {
      memcpy(buffer + *buffer_at, value, copy_len);
      *buffer_at += copy_len;
    }
  }

#ifndef USE_HOST
  const LogString *get_uart_selection_();
#endif

  // Group 4-byte aligned members first
  uint32_t baud_rate_;
  char *tx_buffer_{nullptr};
#if defined(USE_ARDUINO) && !defined(USE_ESP32)
  Stream *hw_serial_{nullptr};
#endif
#if defined(USE_ZEPHYR)
  const device *uart_dev_{nullptr};
#endif
#if defined(USE_ESP32) || defined(USE_LIBRETINY) || defined(USE_ZEPHYR)
  void *main_task_ = nullptr;  // Only used for thread name identification
#endif
#ifdef USE_ESP32
  // Task-specific recursion guards:
  // - Main task uses a dedicated member variable for efficiency
  // - Other tasks use pthread TLS with a dynamically created key via pthread_key_create
  pthread_key_t log_recursion_key_;  // 4 bytes
  uart_port_t uart_num_;             // 4 bytes (enum defaults to int size)
#endif

  // Large objects (internally aligned)
#ifdef USE_LOGGER_RUNTIME_TAG_LEVELS
  std::map<const char *, uint8_t, CStrCompare> log_levels_{};
#endif
  CallbackManager<void(uint8_t, const char *, const char *, size_t)> log_callback_{};
  CallbackManager<void(uint8_t)> level_callback_{};
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  std::unique_ptr<logger::TaskLogBuffer> log_buffer_;  // Will be initialized with init_log_buffer
#endif

  // Group smaller types together at the end
  uint16_t tx_buffer_at_{0};
  uint16_t tx_buffer_size_{0};
  uint8_t current_level_{ESPHOME_LOG_LEVEL_VERY_VERBOSE};
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_ZEPHYR)
  UARTSelection uart_{UART_SELECTION_UART0};
#endif
#ifdef USE_LIBRETINY
  UARTSelection uart_{UART_SELECTION_DEFAULT};
#endif
#ifdef USE_ESP32
  bool main_task_recursion_guard_{false};
#else
  bool global_recursion_guard_{false};  // Simple global recursion guard for single-task platforms
#endif

#if defined(USE_ESP32) || defined(USE_LIBRETINY) || defined(USE_ZEPHYR)
  const char *HOT get_thread_name_(
#ifdef USE_ZEPHYR
      char *buff
#endif
  ) {
#ifdef USE_ZEPHYR
    k_tid_t current_task = k_current_get();
#else
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
#endif
    if (current_task == main_task_) {
      return nullptr;  // Main task
    } else {
#if defined(USE_ESP32)
      return pcTaskGetName(current_task);
#elif defined(USE_LIBRETINY)
      return pcTaskGetTaskName(current_task);
#elif defined(USE_ZEPHYR)
      const char *name = k_thread_name_get(current_task);
      if (name) {
        // zephyr print task names only if debug component is present
        return name;
      }
      std::snprintf(buff, MAX_POINTER_REPRESENTATION, "%p", current_task);
      return buff;
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

  static inline void copy_string(char *buffer, uint16_t &pos, const char *str) {
    const size_t len = strlen(str);
    // Intentionally no null terminator, building larger string
    memcpy(buffer + pos, str, len);  // NOLINT(bugprone-not-null-terminated-result)
    pos += len;
  }

  static inline void write_ansi_color_for_level(char *buffer, uint16_t &pos, uint8_t level) {
    if (level == 0)
      return;
    // Construct ANSI escape sequence: "\033[{bold};3{color}m"
    // Example: "\033[1;31m" for ERROR (bold red)
    buffer[pos++] = '\033';
    buffer[pos++] = '[';
    buffer[pos++] = (level == 1) ? '1' : '0';  // Only ERROR is bold
    buffer[pos++] = ';';
    buffer[pos++] = '3';
    buffer[pos++] = LOG_LEVEL_COLOR_DIGIT[level];
    buffer[pos++] = 'm';
  }

  inline void HOT write_header_to_buffer_(uint8_t level, const char *tag, int line, const char *thread_name,
                                          char *buffer, uint16_t *buffer_at, uint16_t buffer_size) {
    uint16_t pos = *buffer_at;
    // Early return if insufficient space - intentionally don't update buffer_at to prevent partial writes
    if (pos + MAX_HEADER_SIZE > buffer_size)
      return;

    // Construct: <color>[LEVEL][tag:line]:
    write_ansi_color_for_level(buffer, pos, level);
    buffer[pos++] = '[';
    if (level != 0) {
      if (level >= 7) {
        buffer[pos++] = 'V';  // VERY_VERBOSE = "VV"
        buffer[pos++] = 'V';
      } else {
        buffer[pos++] = LOG_LEVEL_LETTER_CHARS[level];
      }
    }
    buffer[pos++] = ']';
    buffer[pos++] = '[';
    copy_string(buffer, pos, tag);
    buffer[pos++] = ':';
    // Format line number without modulo operations (passed by value, safe to mutate)
    if (line > 999) [[unlikely]] {
      int thousands = line / 1000;
      buffer[pos++] = '0' + thousands;
      line -= thousands * 1000;
    }
    int hundreds = line / 100;
    int remainder = line - hundreds * 100;
    int tens = remainder / 10;
    buffer[pos++] = '0' + hundreds;
    buffer[pos++] = '0' + tens;
    buffer[pos++] = '0' + (remainder - tens * 10);
    buffer[pos++] = ']';

#if defined(USE_ESP32) || defined(USE_LIBRETINY) || defined(USE_ZEPHYR)
    if (thread_name != nullptr) {
      write_ansi_color_for_level(buffer, pos, 1);  // Always use bold red for thread name
      buffer[pos++] = '[';
      copy_string(buffer, pos, thread_name);
      buffer[pos++] = ']';
      write_ansi_color_for_level(buffer, pos, level);  // Restore original color
    }
#endif

    buffer[pos++] = ':';
    buffer[pos++] = ' ';
    *buffer_at = pos;
  }

  inline void HOT format_body_to_buffer_(char *buffer, uint16_t *buffer_at, uint16_t buffer_size, const char *format,
                                         va_list args) {
    // Get remaining capacity in the buffer
    if (*buffer_at >= buffer_size)
      return;
    const uint16_t remaining = buffer_size - *buffer_at;

    const int ret = vsnprintf(buffer + *buffer_at, remaining, format, args);

    if (ret < 0) {
      return;  // Encoding error, do not increment buffer_at
    }

    // Update buffer_at with the formatted length (handle truncation)
    uint16_t formatted_len = (ret >= remaining) ? remaining : ret;
    *buffer_at += formatted_len;

    // Remove all trailing newlines right after formatting
    while (*buffer_at > 0 && buffer[*buffer_at - 1] == '\n') {
      (*buffer_at)--;
    }
  }

  inline void HOT write_footer_to_buffer_(char *buffer, uint16_t *buffer_at, uint16_t buffer_size) {
    static constexpr uint16_t RESET_COLOR_LEN = sizeof(ESPHOME_LOG_RESET_COLOR) - 1;
    this->write_body_to_buffer_(ESPHOME_LOG_RESET_COLOR, RESET_COLOR_LEN, buffer, buffer_at, buffer_size);
  }

#ifdef USE_ESP32
  // Disable loop when task buffer is empty (with USB CDC check)
  inline void disable_loop_when_buffer_empty_() {
    // Thread safety note: This is safe even if another task calls enable_loop_soon_any_context()
    // concurrently. If that happens between our check and disable_loop(), the enable request
    // will be processed on the next main loop iteration since:
    // - disable_loop() takes effect immediately
    // - enable_loop_soon_any_context() sets a pending flag that's checked at loop start
    this->disable_loop();
  }
#endif
};
extern Logger *global_logger;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class LoggerMessageTrigger : public Trigger<uint8_t, const char *, const char *> {
 public:
  explicit LoggerMessageTrigger(Logger *parent, uint8_t level) {
    this->level_ = level;
    parent->add_on_log_callback([this](uint8_t level, const char *tag, const char *message, size_t message_len) {
      if (level <= this->level_) {
        this->trigger(level, tag, message);
      }
    });
  }

 protected:
  uint8_t level_;
};

}  // namespace esphome::logger
