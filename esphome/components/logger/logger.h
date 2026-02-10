#pragma once

#include <cstdarg>
#include <map>
#include <span>
#include <type_traits>
#if defined(USE_ESP32) || defined(USE_HOST)
#include <pthread.h>
#endif
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESPHOME_TASK_LOG_BUFFER
#ifdef USE_HOST
#include "task_log_buffer_host.h"
#elif defined(USE_ESP32)
#include "task_log_buffer_esp32.h"
#elif defined(USE_LIBRETINY)
#include "task_log_buffer_libretiny.h"
#endif
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

/** Interface for receiving log messages without std::function overhead.
 *
 * Components can implement this interface instead of using lambdas with std::function
 * to reduce flash usage from std::function type erasure machinery.
 *
 * Usage:
 *   class MyComponent : public Component, public LogListener {
 *    public:
 *     void setup() override {
 *       if (logger::global_logger != nullptr)
 *         logger::global_logger->add_log_listener(this);
 *     }
 *     void on_log(uint8_t level, const char *tag, const char *message, size_t message_len) override {
 *       // Handle log message
 *     }
 *   };
 */
class LogListener {
 public:
  virtual void on_log(uint8_t level, const char *tag, const char *message, size_t message_len) = 0;
};

#ifdef USE_LOGGER_LEVEL_LISTENERS
/** Interface for receiving log level changes without std::function overhead.
 *
 * Components can implement this interface instead of using lambdas with std::function
 * to reduce flash usage from std::function type erasure machinery.
 *
 * Usage:
 *   class MyComponent : public Component, public LoggerLevelListener {
 *    public:
 *     void setup() override {
 *       if (logger::global_logger != nullptr)
 *         logger::global_logger->add_logger_level_listener(this);
 *     }
 *     void on_log_level_change(uint8_t level) override {
 *       // Handle log level change
 *     }
 *   };
 */
class LoggerLevelListener {
 public:
  virtual void on_log_level_change(uint8_t level) = 0;
};
#endif

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

// Stack buffer size for retrieving thread/task names from the OS
// macOS allows up to 64 bytes, Linux up to 16
static constexpr size_t THREAD_NAME_BUF_SIZE = 64;

// Buffer wrapper for log formatting functions
struct LogBuffer {
  char *data;
  uint16_t size;
  uint16_t pos{0};
  // Replaces the null terminator with a newline for console output.
  // Must be called after notify_listeners_() since listeners need null-terminated strings.
  // Console output uses length-based writes (buf.pos), so null terminator is not needed.
  void terminate_with_newline() {
    if (this->pos < this->size) {
      this->data[this->pos++] = '\n';
    } else if (this->size > 0) {
      // Buffer was full - replace last char with newline to ensure it's visible
      this->data[this->size - 1] = '\n';
      this->pos = this->size;
    }
  }
  void HOT write_header(uint8_t level, const char *tag, int line, const char *thread_name) {
    // Early return if insufficient space - intentionally don't update pos to prevent partial writes
    if (this->pos + MAX_HEADER_SIZE > this->size)
      return;

    char *p = this->current_();

    // Write ANSI color
    this->write_ansi_color_(p, level);

    // Construct: [LEVEL][tag:line]
    *p++ = '[';
    if (level != 0) {
      if (level >= 7) {
        *p++ = 'V';  // VERY_VERBOSE = "VV"
        *p++ = 'V';
      } else {
        *p++ = LOG_LEVEL_LETTER_CHARS[level];
      }
    }
    *p++ = ']';
    *p++ = '[';

    // Copy tag
    this->copy_string_(p, tag);

    *p++ = ':';

    // Format line number without modulo operations
    if (line > 999) [[unlikely]] {
      int thousands = line / 1000;
      *p++ = '0' + thousands;
      line -= thousands * 1000;
    }
    int hundreds = line / 100;
    int remainder = line - hundreds * 100;
    int tens = remainder / 10;
    *p++ = '0' + hundreds;
    *p++ = '0' + tens;
    *p++ = '0' + (remainder - tens * 10);
    *p++ = ']';

#if defined(USE_ESP32) || defined(USE_LIBRETINY) || defined(USE_ZEPHYR) || defined(USE_HOST)
    // Write thread name with bold red color
    if (thread_name != nullptr) {
      this->write_ansi_color_(p, 1);  // Bold red for thread name
      *p++ = '[';
      this->copy_string_(p, thread_name);
      *p++ = ']';
      this->write_ansi_color_(p, level);  // Restore original color
    }
#endif

    *p++ = ':';
    *p++ = ' ';

    this->pos = p - this->data;
  }
  void HOT format_body(const char *format, va_list args) {
    this->format_vsnprintf_(format, args);
    this->finalize_();
  }
#ifdef USE_STORE_LOG_STR_IN_FLASH
  void HOT format_body_P(PGM_P format, va_list args) {
    this->format_vsnprintf_P_(format, args);
    this->finalize_();
  }
#endif
  void write_body(const char *text, uint16_t text_length) {
    this->write_(text, text_length);
    this->finalize_();
  }

 private:
  bool full_() const { return this->pos >= this->size; }
  uint16_t remaining_() const { return this->size - this->pos; }
  char *current_() { return this->data + this->pos; }
  void write_(const char *value, uint16_t length) {
    const uint16_t available = this->remaining_();
    const uint16_t copy_len = (length < available) ? length : available;
    if (copy_len > 0) {
      memcpy(this->current_(), value, copy_len);
      this->pos += copy_len;
    }
  }
  void finalize_() {
    // Write color reset sequence
    static constexpr uint16_t RESET_COLOR_LEN = sizeof(ESPHOME_LOG_RESET_COLOR) - 1;
    this->write_(ESPHOME_LOG_RESET_COLOR, RESET_COLOR_LEN);
    // Null terminate
    this->data[this->full_() ? this->size - 1 : this->pos] = '\0';
  }
  void strip_trailing_newlines_() {
    while (this->pos > 0 && this->data[this->pos - 1] == '\n')
      this->pos--;
  }
  void process_vsnprintf_result_(int ret) {
    if (ret < 0)
      return;
    const uint16_t rem = this->remaining_();
    this->pos += (ret >= rem) ? (rem - 1) : static_cast<uint16_t>(ret);
    this->strip_trailing_newlines_();
  }
  void format_vsnprintf_(const char *format, va_list args) {
    if (this->full_())
      return;
    this->process_vsnprintf_result_(vsnprintf(this->current_(), this->remaining_(), format, args));
  }
#ifdef USE_STORE_LOG_STR_IN_FLASH
  void format_vsnprintf_P_(PGM_P format, va_list args) {
    if (this->full_())
      return;
    this->process_vsnprintf_result_(vsnprintf_P(this->current_(), this->remaining_(), format, args));
  }
#endif
  // Write ANSI color escape sequence to buffer, updates pointer in place
  // Caller is responsible for ensuring buffer has sufficient space
  void write_ansi_color_(char *&p, uint8_t level) {
    if (level == 0)
      return;
    // Direct buffer fill: "\033[{bold};3{color}m" (7 bytes)
    *p++ = '\033';
    *p++ = '[';
    *p++ = (level == 1) ? '1' : '0';  // Only ERROR is bold
    *p++ = ';';
    *p++ = '3';
    *p++ = LOG_LEVEL_COLOR_DIGIT[level];
    *p++ = 'm';
  }
  // Copy string without null terminator, updates pointer in place
  // Caller is responsible for ensuring buffer has sufficient space
  void copy_string_(char *&p, const char *str) {
    const size_t len = strlen(str);
    // NOLINTNEXTLINE(bugprone-not-null-terminated-result) - intentionally no null terminator, building string piece by
    // piece
    memcpy(p, str, len);
    p += len;
  }
};

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
#ifdef USE_HOST
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

#ifdef USE_LOG_LISTENERS
  /// Register a log listener to receive log messages
  void add_log_listener(LogListener *listener) { this->log_listeners_.push_back(listener); }
#else
  /// No-op when log listeners are disabled
  void add_log_listener(LogListener *listener) {}
#endif

#ifdef USE_LOGGER_LEVEL_LISTENERS
  /// Register a listener for log level changes
  void add_level_listener(LoggerLevelListener *listener) { this->level_listeners_.push_back(listener); }
#endif

  float get_setup_priority() const override;

  void log_vprintf_(uint8_t level, const char *tag, int line, const char *format, va_list args);  // NOLINT
#ifdef USE_STORE_LOG_STR_IN_FLASH
  void log_vprintf_(uint8_t level, const char *tag, int line, const __FlashStringHelper *format,
                    va_list args);  // NOLINT
#endif

 protected:
  // RAII guard for recursion flags - sets flag on construction, clears on destruction
  class RecursionGuard {
   public:
    explicit RecursionGuard(bool &flag) : flag_(flag) { flag_ = true; }
    ~RecursionGuard() { flag_ = false; }
    RecursionGuard(const RecursionGuard &) = delete;
    RecursionGuard &operator=(const RecursionGuard &) = delete;
    RecursionGuard(RecursionGuard &&) = delete;
    RecursionGuard &operator=(RecursionGuard &&) = delete;

   private:
    bool &flag_;
  };

#if defined(USE_ESP32) || defined(USE_HOST) || defined(USE_LIBRETINY)
  // Handles non-main thread logging only (~0.1% of calls)
  // thread_name is resolved by the caller from the task handle, avoiding redundant lookups
  void log_vprintf_non_main_thread_(uint8_t level, const char *tag, int line, const char *format, va_list args,
                                    const char *thread_name);
#endif
  void process_messages_();
  void write_msg_(const char *msg, uint16_t len);

  // Format a log message with printf-style arguments and write it to a buffer with header, footer, and null terminator
  // thread_name: name of the calling thread/task, or nullptr for main task (callers already know which task they're on)
  inline void HOT format_log_to_buffer_with_terminator_(uint8_t level, const char *tag, int line, const char *format,
                                                        va_list args, LogBuffer &buf, const char *thread_name) {
    buf.write_header(level, tag, line, thread_name);
    buf.format_body(format, args);
  }

#ifdef USE_STORE_LOG_STR_IN_FLASH
  // Format a log message with flash string format and write it to a buffer with header, footer, and null terminator
  // ESP8266-only (single-task), thread_name is always nullptr
  inline void HOT format_log_to_buffer_with_terminator_P_(uint8_t level, const char *tag, int line,
                                                          const __FlashStringHelper *format, va_list args,
                                                          LogBuffer &buf) {
    buf.write_header(level, tag, line, nullptr);
    buf.format_body_P(reinterpret_cast<PGM_P>(format), args);
  }
#endif

  // Helper to notify log listeners
  inline void HOT notify_listeners_(uint8_t level, const char *tag, const LogBuffer &buf) {
#ifdef USE_LOG_LISTENERS
    for (auto *listener : this->log_listeners_)
      listener->on_log(level, tag, buf.data, buf.pos);
#endif
  }

  // Helper to write log buffer to console (replaces null terminator with newline and writes)
  inline void HOT write_to_console_(LogBuffer &buf) {
    buf.terminate_with_newline();
    this->write_msg_(buf.data, buf.pos);
  }

  // Helper to write log buffer to console if logging is enabled
  inline void HOT write_log_buffer_to_console_(LogBuffer &buf) {
    if (this->baud_rate_ > 0)
      this->write_to_console_(buf);
  }

  // Helper to format and send a log message to both console and listeners
  // Template handles both const char* (RAM) and __FlashStringHelper* (flash) format strings
  // thread_name: name of the calling thread/task, or nullptr for main task
  template<typename FormatType>
  inline void HOT log_message_to_buffer_and_send_(bool &recursion_guard, uint8_t level, const char *tag, int line,
                                                  FormatType format, va_list args, const char *thread_name) {
    RecursionGuard guard(recursion_guard);
    LogBuffer buf{this->tx_buffer_, this->tx_buffer_size_};
#ifdef USE_STORE_LOG_STR_IN_FLASH
    if constexpr (std::is_same_v<FormatType, const __FlashStringHelper *>) {
      this->format_log_to_buffer_with_terminator_P_(level, tag, line, format, args, buf);
    } else
#endif
    {
      this->format_log_to_buffer_with_terminator_(level, tag, line, format, args, buf, thread_name);
    }
    this->notify_listeners_(level, tag, buf);
    this->write_log_buffer_to_console_(buf);
  }

#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  // Helper to format a pre-formatted message from the task log buffer and notify listeners
  // Used by process_messages_ to avoid code duplication between ESP32 and host platforms
  inline void HOT format_buffered_message_and_notify_(uint8_t level, const char *tag, uint16_t line,
                                                      const char *thread_name, const char *text, uint16_t text_length,
                                                      LogBuffer &buf) {
    buf.write_header(level, tag, line, thread_name);
    buf.write_body(text, text_length);
    this->notify_listeners_(level, tag, buf);
  }
#endif

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
  void *main_task_{nullptr};  // Main thread/task for fast path comparison
#endif
#ifdef USE_HOST
  pthread_t main_thread_{};  // Main thread for pthread_equal() comparison
#endif
#ifdef USE_ESP32
  // Task-specific recursion guards:
  // - Main task uses a dedicated member variable for efficiency
  // - Other tasks use pthread TLS with a dynamically created key via pthread_key_create
  pthread_key_t log_recursion_key_;  // 4 bytes
  uart_port_t uart_num_;             // 4 bytes (enum defaults to int size)
#endif
#ifdef USE_HOST
  // Thread-specific recursion guards using pthread TLS
  pthread_key_t log_recursion_key_;
#endif

  // Large objects (internally aligned)
#ifdef USE_LOGGER_RUNTIME_TAG_LEVELS
  std::map<const char *, uint8_t, CStrCompare> log_levels_{};
#endif
#ifdef USE_LOG_LISTENERS
  StaticVector<LogListener *, ESPHOME_LOG_MAX_LISTENERS>
      log_listeners_;  // Log message listeners (API, MQTT, syslog, etc.)
#endif
#ifdef USE_LOGGER_LEVEL_LISTENERS
  std::vector<LoggerLevelListener *> level_listeners_;  // Log level change listeners
#endif
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
#ifdef USE_HOST
  logger::TaskLogBufferHost *log_buffer_{nullptr};  // Allocated once, never freed
#elif defined(USE_ESP32)
  logger::TaskLogBuffer *log_buffer_{nullptr};  // Allocated once, never freed
#elif defined(USE_LIBRETINY)
  logger::TaskLogBufferLibreTiny *log_buffer_{nullptr};  // Allocated once, never freed
#endif
#endif

  // Group smaller types together at the end
  uint16_t tx_buffer_size_{0};
  uint8_t current_level_{ESPHOME_LOG_LEVEL_VERY_VERBOSE};
#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_ZEPHYR)
  UARTSelection uart_{UART_SELECTION_UART0};
#endif
#ifdef USE_LIBRETINY
  UARTSelection uart_{UART_SELECTION_DEFAULT};
#endif
#if defined(USE_ESP32) || defined(USE_HOST) || defined(USE_LIBRETINY)
  bool main_task_recursion_guard_{false};
#ifdef USE_LIBRETINY
  bool non_main_task_recursion_guard_{false};  // Shared guard for all non-main tasks on LibreTiny
#endif
#else
  bool global_recursion_guard_{false};  // Simple global recursion guard for single-task platforms
#endif

  // --- get_thread_name_ overloads (per-platform) ---

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  // Primary overload - takes a task handle directly to avoid redundant xTaskGetCurrentTaskHandle() calls
  // when the caller already has the handle (e.g. from the main task check in log_vprintf_)
  const char *get_thread_name_(TaskHandle_t task) {
    if (task == this->main_task_) {
      return nullptr;  // Main task
    }
#if defined(USE_ESP32)
    return pcTaskGetName(task);
#elif defined(USE_LIBRETINY)
    return pcTaskGetTaskName(task);
#endif
  }

  // Convenience overload - gets the current task handle and delegates
  const char *HOT get_thread_name_() { return this->get_thread_name_(xTaskGetCurrentTaskHandle()); }

#elif defined(USE_HOST)
  // Takes a caller-provided buffer for the thread name (stack-allocated for thread safety)
  const char *HOT get_thread_name_(std::span<char> buff) {
    pthread_t current_thread = pthread_self();
    if (pthread_equal(current_thread, main_thread_)) {
      return nullptr;  // Main thread
    }
    // For non-main threads, get the thread name into the caller-provided buffer
    if (pthread_getname_np(current_thread, buff.data(), buff.size()) == 0) {
      return buff.data();
    }
    return nullptr;
  }

#elif defined(USE_ZEPHYR)
  const char *HOT get_thread_name_(std::span<char> buff) {
    k_tid_t current_task = k_current_get();
    if (current_task == main_task_) {
      return nullptr;  // Main task
    }
    const char *name = k_thread_name_get(current_task);
    if (name) {
      // zephyr print task names only if debug component is present
      return name;
    }
    std::snprintf(buff.data(), buff.size(), "%p", current_task);
    return buff.data();
  }
#endif

  // --- Non-main task recursion guards (per-platform) ---

#if defined(USE_ESP32) || defined(USE_HOST)
  // RAII guard for non-main task recursion using pthread TLS
  class NonMainTaskRecursionGuard {
   public:
    explicit NonMainTaskRecursionGuard(pthread_key_t key) : key_(key) {
      pthread_setspecific(key_, reinterpret_cast<void *>(1));
    }
    ~NonMainTaskRecursionGuard() { pthread_setspecific(key_, nullptr); }
    NonMainTaskRecursionGuard(const NonMainTaskRecursionGuard &) = delete;
    NonMainTaskRecursionGuard &operator=(const NonMainTaskRecursionGuard &) = delete;
    NonMainTaskRecursionGuard(NonMainTaskRecursionGuard &&) = delete;
    NonMainTaskRecursionGuard &operator=(NonMainTaskRecursionGuard &&) = delete;

   private:
    pthread_key_t key_;
  };

  // Check if non-main task is already in recursion (via TLS)
  inline bool HOT is_non_main_task_recursive_() const { return pthread_getspecific(log_recursion_key_) != nullptr; }

  // Create RAII guard for non-main task recursion
  inline NonMainTaskRecursionGuard make_non_main_task_guard_() { return NonMainTaskRecursionGuard(log_recursion_key_); }

#elif defined(USE_LIBRETINY)
  // LibreTiny doesn't have FreeRTOS TLS, so use a simple approach:
  // - Main task uses dedicated boolean (same as ESP32)
  // - Non-main tasks share a single recursion guard
  // This is safe because:
  // - Recursion from logging within logging is the main concern
  // - Cross-task "recursion" is prevented by the buffer mutex anyway
  // - Missing a recursive call from another task is acceptable (falls back to direct output)

  // Check if non-main task is already in recursion
  inline bool HOT is_non_main_task_recursive_() const { return non_main_task_recursion_guard_; }

  // Create RAII guard for non-main task recursion (uses shared boolean for all non-main tasks)
  inline RecursionGuard make_non_main_task_guard_() { return RecursionGuard(non_main_task_recursion_guard_); }
#endif

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  // Disable loop when task buffer is empty (with USB CDC check on ESP32)
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

class LoggerMessageTrigger : public Trigger<uint8_t, const char *, const char *>, public LogListener {
 public:
  explicit LoggerMessageTrigger(Logger *parent, uint8_t level) : level_(level) { parent->add_log_listener(this); }

  void on_log(uint8_t level, const char *tag, const char *message, size_t message_len) override {
    (void) message_len;
    if (level <= this->level_) {
      this->trigger(level, tag, message);
    }
  }

 protected:
  uint8_t level_;
};

}  // namespace esphome::logger
