#include "logger.h"
#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::logger {

static const char *const TAG = "logger";

#if defined(USE_ESP32) || defined(USE_HOST) || defined(USE_LIBRETINY) || defined(USE_ZEPHYR)
// Implementation for multi-threaded platforms (ESP32 with FreeRTOS, Host with pthreads, LibreTiny with FreeRTOS,
// Zephyr) Main thread/task always uses direct buffer access for console output and callbacks
//
// For non-main threads/tasks:
//  - WITH task log buffer: Prefer sending to ring buffer for async processing
//    - Avoids allocating stack memory for console output in normal operation
//    - Prevents console corruption from concurrent writes by multiple threads
//    - Messages are serialized through main loop for proper console output
//    - Fallback to emergency console logging only if ring buffer is full
//  - WITHOUT task log buffer: Only emergency console output, no callbacks
//
// Optimized for the common case: 99.9% of logs come from the main thread
void HOT Logger::log_vprintf_(uint8_t level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
  if (level > this->level_for(tag))
    return;

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  // Get task handle once - used for both main task check and passing to non-main thread handler
  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  const bool is_main_task = (current_task == this->main_task_);
#elif (USE_ZEPHYR)
  k_tid_t current_task = k_current_get();
  const bool is_main_task = (current_task == this->main_task_);
#else  // USE_HOST
  const bool is_main_task = pthread_equal(pthread_self(), this->main_thread_);
#endif

  // Fast path: main thread, no recursion (99.9% of all logs)
  // Pass nullptr for thread_name since we already know this is the main task
  if (is_main_task && !this->main_task_recursion_guard_) [[likely]] {
    this->log_message_to_buffer_and_send_(this->main_task_recursion_guard_, level, tag, line, format, args, nullptr);
    return;
  }

  // Main task with recursion - silently drop to prevent infinite loop
  if (is_main_task) {
    return;
  }

  // Non-main thread handling (~0.1% of logs)
  // Resolve thread name once and pass it through the logging chain.
  // ESP32/LibreTiny: use TaskHandle_t overload to avoid redundant xTaskGetCurrentTaskHandle()
  // (we already have the handle from the main task check above).
  // Host: pass a stack buffer for pthread_getname_np to write into.
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  const char *thread_name = get_thread_name_(current_task);
#elif defined(USE_ZEPHYR)
  char thread_name_buf[MAX_POINTER_REPRESENTATION];
  const char *thread_name = get_thread_name_(thread_name_buf, current_task);
#else  // USE_HOST
  char thread_name_buf[THREAD_NAME_BUF_SIZE];
  const char *thread_name = this->get_thread_name_(thread_name_buf);
#endif
  this->log_vprintf_non_main_thread_(level, tag, line, format, args, thread_name);
}

// Handles non-main thread logging only
// Kept separate from hot path to improve instruction cache performance
void Logger::log_vprintf_non_main_thread_(uint8_t level, const char *tag, int line, const char *format, va_list args,
                                          const char *thread_name) {
  // Check if already in recursion for this non-main thread/task
  if (this->is_non_main_task_recursive_()) {
    return;
  }

  // RAII guard - automatically resets on any return path
  auto guard = this->make_non_main_task_guard_();

  bool message_sent = false;
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  // For non-main threads/tasks, queue the message for callbacks
  message_sent =
      this->log_buffer_->send_message_thread_safe(level, tag, static_cast<uint16_t>(line), thread_name, format, args);
  if (message_sent) {
    // Enable logger loop to process the buffered message
    // This is safe to call from any context including ISRs
    this->enable_loop_soon_any_context();
  }
#endif
  // Emergency console logging for non-main threads when ring buffer is full or disabled
  // This is a fallback mechanism to ensure critical log messages are visible
  // Note: This may cause interleaved/corrupted console output if multiple threads
  // log simultaneously, but it's better than losing important messages entirely
#ifdef USE_HOST
  if (!message_sent)
#else
  if (!message_sent && this->baud_rate_ > 0)  // If logging is enabled, write to console
#endif
  {
#ifdef USE_HOST
    // Host always has console output - no baud_rate check needed
    static const size_t MAX_CONSOLE_LOG_MSG_SIZE = 512;
#else
    // Maximum size for console log messages (includes null terminator)
    static const size_t MAX_CONSOLE_LOG_MSG_SIZE = 144;
#endif
    char console_buffer[MAX_CONSOLE_LOG_MSG_SIZE];  // MUST be stack allocated for thread safety
    LogBuffer buf{console_buffer, MAX_CONSOLE_LOG_MSG_SIZE};
    this->format_log_to_buffer_with_terminator_(level, tag, line, format, args, buf, thread_name);
    this->write_to_console_(buf);
  }

  // RAII guard automatically resets on return
}
#else
// Implementation for single-task platforms (ESP8266, RP2040)
// Logging calls are NOT thread-safe: global_recursion_guard_ is a plain bool and tx_buffer_ has no locking.
// Not a problem in practice yet since Zephyr has no API support (logs are console-only).
void HOT Logger::log_vprintf_(uint8_t level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
  if (level > this->level_for(tag) || global_recursion_guard_)
    return;
  // Other single-task platforms don't have thread names, so pass nullptr
  this->log_message_to_buffer_and_send_(global_recursion_guard_, level, tag, line, format, args, nullptr);
}
#endif  // USE_ESP32 || USE_HOST || USE_LIBRETINY || USE_ZEPHYR

#ifdef USE_STORE_LOG_STR_IN_FLASH
// Implementation for ESP8266 with flash string support.
// Note: USE_STORE_LOG_STR_IN_FLASH is only defined for ESP8266.
//
// This function handles format strings stored in flash memory (PROGMEM) to save RAM.
// Uses vsnprintf_P to read the format string directly from flash without copying to RAM.
//
void Logger::log_vprintf_(uint8_t level, const char *tag, int line, const __FlashStringHelper *format,
                          va_list args) {  // NOLINT
  if (level > this->level_for(tag) || global_recursion_guard_)
    return;

  this->log_message_to_buffer_and_send_(global_recursion_guard_, level, tag, line, format, args, nullptr);
}
#endif  // USE_STORE_LOG_STR_IN_FLASH

inline uint8_t Logger::level_for(const char *tag) {
#ifdef USE_LOGGER_RUNTIME_TAG_LEVELS
  auto it = this->log_levels_.find(tag);
  if (it != this->log_levels_.end())
    return it->second;
#endif
  return this->current_level_;
}

Logger::Logger(uint32_t baud_rate, size_t tx_buffer_size) : baud_rate_(baud_rate), tx_buffer_size_(tx_buffer_size) {
  // add 1 to buffer size for null terminator
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) - allocated once, never freed
  this->tx_buffer_ = new char[this->tx_buffer_size_ + 1];
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  this->main_task_ = xTaskGetCurrentTaskHandle();
#elif defined(USE_ZEPHYR)
  this->main_task_ = k_current_get();
#elif defined(USE_HOST)
  this->main_thread_ = pthread_self();
#endif
}
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
void Logger::init_log_buffer(size_t total_buffer_size) {
  // Host uses slot count instead of byte size
  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory) - allocated once, never freed
  this->log_buffer_ = new logger::TaskLogBuffer(total_buffer_size);

// Zephyr needs loop working to check when CDC port is open
#if !(defined(USE_ZEPHYR) || defined(USE_LOGGER_USB_CDC))
  // Start with loop disabled when using task buffer (unless using USB CDC on ESP32)
  // The loop will be enabled automatically when messages arrive
  this->disable_loop_when_buffer_empty_();
#endif
}
#endif

#if defined(USE_ESPHOME_TASK_LOG_BUFFER) || (defined(USE_ZEPHYR) && defined(USE_LOGGER_USB_CDC))
void Logger::loop() {
  this->process_messages_();
#if defined(USE_ZEPHYR) && defined(USE_LOGGER_USB_CDC)
  this->cdc_loop_();
#endif
}
#endif

void Logger::process_messages_() {
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  // Process any buffered messages when available
  if (this->log_buffer_->has_messages()) {
    logger::TaskLogBuffer::LogMessage *message;
    uint16_t text_length;
    while (this->log_buffer_->borrow_message_main_loop(message, text_length)) {
      const char *thread_name = message->thread_name[0] != '\0' ? message->thread_name : nullptr;
      LogBuffer buf{this->tx_buffer_, this->tx_buffer_size_};
      this->format_buffered_message_and_notify_(message->level, message->tag, message->line, thread_name,
                                                message->text_data(), text_length, buf);
      // Release the message to allow other tasks to use it as soon as possible
      this->log_buffer_->release_message_main_loop();
      this->write_log_buffer_to_console_(buf);
    }
  }
// Zephyr needs loop working to check when CDC port is open
#if !(defined(USE_ZEPHYR) || defined(USE_LOGGER_USB_CDC))
  else {
    // No messages to process, disable loop if appropriate
    // This reduces overhead when there's no async logging activity
    this->disable_loop_when_buffer_empty_();
  }
#endif
#endif  // USE_ESPHOME_TASK_LOG_BUFFER
}

void Logger::set_baud_rate(uint32_t baud_rate) { this->baud_rate_ = baud_rate; }
#ifdef USE_LOGGER_RUNTIME_TAG_LEVELS
void Logger::set_log_level(const char *tag, uint8_t log_level) { this->log_levels_[tag] = log_level; }
#endif

#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY) || defined(USE_ZEPHYR)
UARTSelection Logger::get_uart() const { return this->uart_; }
#endif

float Logger::get_setup_priority() const { return setup_priority::BUS + 500.0f; }

// Log level strings - packed into flash on ESP8266, indexed by log level (0-7)
PROGMEM_STRING_TABLE(LogLevelStrings, "NONE", "ERROR", "WARN", "INFO", "CONFIG", "DEBUG", "VERBOSE", "VERY_VERBOSE");

static const LogString *get_log_level_str(uint8_t level) {
  return LogLevelStrings::get_log_str(level, LogLevelStrings::LAST_INDEX);
}

void Logger::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Logger:\n"
                "  Max Level: %s\n"
                "  Initial Level: %s",
                LOG_STR_ARG(get_log_level_str(ESPHOME_LOG_LEVEL)),
                LOG_STR_ARG(get_log_level_str(this->current_level_)));
#ifndef USE_HOST
  ESP_LOGCONFIG(TAG,
                "  Log Baud Rate: %" PRIu32 "\n"
                "  Hardware UART: %s",
                this->baud_rate_, LOG_STR_ARG(get_uart_selection_()));
#endif
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  if (this->log_buffer_) {
#ifdef USE_HOST
    ESP_LOGCONFIG(TAG, "  Task Log Buffer Slots: %u", static_cast<unsigned int>(this->log_buffer_->size()));
#else
    ESP_LOGCONFIG(TAG, "  Task Log Buffer Size: %u bytes", static_cast<unsigned int>(this->log_buffer_->size()));
#endif
  }
#endif

#ifdef USE_LOGGER_RUNTIME_TAG_LEVELS
  for (auto &it : this->log_levels_) {
    ESP_LOGCONFIG(TAG, "  Level for '%s': %s", it.first, LOG_STR_ARG(get_log_level_str(it.second)));
  }
#endif
}

void Logger::set_log_level(uint8_t level) {
  if (level > ESPHOME_LOG_LEVEL) {
    level = ESPHOME_LOG_LEVEL;
    ESP_LOGW(TAG, "Cannot set log level higher than pre-compiled %s",
             LOG_STR_ARG(get_log_level_str(ESPHOME_LOG_LEVEL)));
  }
  this->current_level_ = level;
#ifdef USE_LOGGER_LEVEL_LISTENERS
  for (auto *listener : this->level_listeners_)
    listener->on_log_level_change(level);
#endif
}

Logger *global_logger = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::logger
