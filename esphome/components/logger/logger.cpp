#include "logger.h"
#include <cinttypes>
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
#include <memory>  // For unique_ptr
#endif

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::logger {

static const char *const TAG = "logger";

#ifdef USE_ESP32
// Implementation for ESP32 (multi-task platform with task-specific tracking)
// Main task always uses direct buffer access for console output and callbacks
//
// For non-main tasks:
//  - WITH task log buffer: Prefer sending to ring buffer for async processing
//    - Avoids allocating stack memory for console output in normal operation
//    - Prevents console corruption from concurrent writes by multiple tasks
//    - Messages are serialized through main loop for proper console output
//    - Fallback to emergency console logging only if ring buffer is full
//  - WITHOUT task log buffer: Only emergency console output, no callbacks
void HOT Logger::log_vprintf_(uint8_t level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
  if (level > this->level_for(tag))
    return;

  TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
  bool is_main_task = (current_task == main_task_);

  // Check and set recursion guard - uses pthread TLS for per-task state
  if (this->check_and_set_task_log_recursion_(is_main_task)) {
    return;  // Recursion detected
  }

  // Main task uses the shared buffer for efficiency
  if (is_main_task) {
    this->log_message_to_buffer_and_send_(level, tag, line, format, args);
    this->reset_task_log_recursion_(is_main_task);
    return;
  }

  bool message_sent = false;
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  // For non-main tasks, queue the message for callbacks - but only if we have any callbacks registered
  message_sent =
      this->log_buffer_->send_message_thread_safe(level, tag, static_cast<uint16_t>(line), current_task, format, args);
  if (message_sent) {
    // Enable logger loop to process the buffered message
    // This is safe to call from any context including ISRs
    this->enable_loop_soon_any_context();
  }
#endif  // USE_ESPHOME_TASK_LOG_BUFFER

  // Emergency console logging for non-main tasks when ring buffer is full or disabled
  // This is a fallback mechanism to ensure critical log messages are visible
  // Note: This may cause interleaved/corrupted console output if multiple tasks
  // log simultaneously, but it's better than losing important messages entirely
  if (!message_sent && this->baud_rate_ > 0) {  // If logging is enabled, write to console
    // Maximum size for console log messages (includes null terminator)
    static const size_t MAX_CONSOLE_LOG_MSG_SIZE = 144;
    char console_buffer[MAX_CONSOLE_LOG_MSG_SIZE];  // MUST be stack allocated for thread safety
    uint16_t buffer_at = 0;                         // Initialize buffer position
    this->format_log_to_buffer_with_terminator_(level, tag, line, format, args, console_buffer, &buffer_at,
                                                MAX_CONSOLE_LOG_MSG_SIZE);
    // Add newline if platform needs it (ESP32 doesn't add via write_msg_)
    this->add_newline_to_buffer_if_needed_(console_buffer, &buffer_at, MAX_CONSOLE_LOG_MSG_SIZE);
    this->write_msg_(console_buffer, buffer_at);
  }

  // Reset the recursion guard for this task
  this->reset_task_log_recursion_(is_main_task);
}
#else
// Implementation for all other platforms
void HOT Logger::log_vprintf_(uint8_t level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
  if (level > this->level_for(tag) || global_recursion_guard_)
    return;

  global_recursion_guard_ = true;

  // Format and send to both console and callbacks
  this->log_message_to_buffer_and_send_(level, tag, line, format, args);

  global_recursion_guard_ = false;
}
#endif  // !USE_ESP32

#ifdef USE_STORE_LOG_STR_IN_FLASH
// Implementation for ESP8266 with flash string support.
// Note: USE_STORE_LOG_STR_IN_FLASH is only defined for ESP8266.
//
// This function handles format strings stored in flash memory (PROGMEM) to save RAM.
// The buffer is used in a special way to avoid allocating extra memory:
//
// Memory layout during execution:
// Step 1: Copy format string from flash to buffer
//         tx_buffer_: [format_string][null][.....................]
//         tx_buffer_at_: ------------------^
//         msg_start: saved here -----------^
//
// Step 2: format_log_to_buffer_with_terminator_ reads format string from beginning
//         and writes formatted output starting at msg_start position
//         tx_buffer_: [format_string][null][formatted_message][null]
//         tx_buffer_at_: -------------------------------------^
//
// Step 3: Output the formatted message (starting at msg_start)
//         write_msg_ and callbacks receive: this->tx_buffer_ + msg_start
//         which points to: [formatted_message][null]
//
void Logger::log_vprintf_(uint8_t level, const char *tag, int line, const __FlashStringHelper *format,
                          va_list args) {  // NOLINT
  if (level > this->level_for(tag) || global_recursion_guard_)
    return;

  global_recursion_guard_ = true;
  this->tx_buffer_at_ = 0;

  // Copy format string from progmem
  auto *format_pgm_p = reinterpret_cast<const uint8_t *>(format);
  char ch = '.';
  while (this->tx_buffer_at_ < this->tx_buffer_size_ && ch != '\0') {
    this->tx_buffer_[this->tx_buffer_at_++] = ch = (char) progmem_read_byte(format_pgm_p++);
  }

  // Buffer full from copying format
  if (this->tx_buffer_at_ >= this->tx_buffer_size_) {
    global_recursion_guard_ = false;  // Make sure to reset the recursion guard before returning
    return;
  }

  // Save the offset before calling format_log_to_buffer_with_terminator_
  // since it will increment tx_buffer_at_ to the end of the formatted string
  uint16_t msg_start = this->tx_buffer_at_;
  this->format_log_to_buffer_with_terminator_(level, tag, line, this->tx_buffer_, args, this->tx_buffer_,
                                              &this->tx_buffer_at_, this->tx_buffer_size_);

  uint16_t msg_length =
      this->tx_buffer_at_ - msg_start;  // Don't subtract 1 - tx_buffer_at_ is already at the null terminator position

  // Listeners get message first (before console write)
  for (auto *listener : this->log_listeners_)
    listener->on_log(level, tag, this->tx_buffer_ + msg_start, msg_length);

  // Write to console starting at the msg_start
  this->write_tx_buffer_to_console_(msg_start, &msg_length);

  global_recursion_guard_ = false;
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
  this->tx_buffer_ = new char[this->tx_buffer_size_ + 1];  // NOLINT
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  this->main_task_ = xTaskGetCurrentTaskHandle();
#elif defined(USE_ZEPHYR)
  this->main_task_ = k_current_get();
#endif
}
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
void Logger::init_log_buffer(size_t total_buffer_size) {
  this->log_buffer_ = esphome::make_unique<logger::TaskLogBuffer>(total_buffer_size);

  // Start with loop disabled when using task buffer (unless using USB CDC)
  // The loop will be enabled automatically when messages arrive
  this->disable_loop_when_buffer_empty_();
}
#endif

#ifdef USE_ESPHOME_TASK_LOG_BUFFER
void Logger::loop() { this->process_messages_(); }
#endif

void Logger::process_messages_() {
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  // Process any buffered messages when available
  if (this->log_buffer_->has_messages()) {
    logger::TaskLogBuffer::LogMessage *message;
    const char *text;
    void *received_token;

    // Process messages from the buffer
    while (this->log_buffer_->borrow_message_main_loop(&message, &text, &received_token)) {
      this->tx_buffer_at_ = 0;
      // Use the thread name that was stored when the message was created
      // This avoids potential crashes if the task no longer exists
      const char *thread_name = message->thread_name[0] != '\0' ? message->thread_name : nullptr;
      this->write_header_to_buffer_(message->level, message->tag, message->line, thread_name, this->tx_buffer_,
                                    &this->tx_buffer_at_, this->tx_buffer_size_);
      this->write_body_to_buffer_(text, message->text_length, this->tx_buffer_, &this->tx_buffer_at_,
                                  this->tx_buffer_size_);
      this->write_footer_to_buffer_(this->tx_buffer_, &this->tx_buffer_at_, this->tx_buffer_size_);
      this->tx_buffer_[this->tx_buffer_at_] = '\0';
      size_t msg_len = this->tx_buffer_at_;  // We already know the length from tx_buffer_at_
      for (auto *listener : this->log_listeners_)
        listener->on_log(message->level, message->tag, this->tx_buffer_, msg_len);
      // At this point all the data we need from message has been transferred to the tx_buffer
      // so we can release the message to allow other tasks to use it as soon as possible.
      this->log_buffer_->release_message_main_loop(received_token);

      // Write to console from the main loop to prevent corruption from concurrent writes
      // This ensures all log messages appear on the console in a clean, serialized manner
      // Note: Messages may appear slightly out of order due to async processing, but
      // this is preferred over corrupted/interleaved console output
      this->write_tx_buffer_to_console_();
    }
  } else {
    // No messages to process, disable loop if appropriate
    // This reduces overhead when there's no async logging activity
    this->disable_loop_when_buffer_empty_();
  }
#endif
}

void Logger::set_baud_rate(uint32_t baud_rate) { this->baud_rate_ = baud_rate; }
#ifdef USE_LOGGER_RUNTIME_TAG_LEVELS
void Logger::set_log_level(const char *tag, uint8_t log_level) { this->log_levels_[tag] = log_level; }
#endif

#if defined(USE_ESP32) || defined(USE_ESP8266) || defined(USE_RP2040) || defined(USE_LIBRETINY) || defined(USE_ZEPHYR)
UARTSelection Logger::get_uart() const { return this->uart_; }
#endif

float Logger::get_setup_priority() const { return setup_priority::BUS + 500.0f; }

#ifdef USE_STORE_LOG_STR_IN_FLASH
// ESP8266: PSTR() cannot be used in array initializers, so we need to declare
// each string separately as a global constant first
static const char LOG_LEVEL_NONE[] PROGMEM = "NONE";
static const char LOG_LEVEL_ERROR[] PROGMEM = "ERROR";
static const char LOG_LEVEL_WARN[] PROGMEM = "WARN";
static const char LOG_LEVEL_INFO[] PROGMEM = "INFO";
static const char LOG_LEVEL_CONFIG[] PROGMEM = "CONFIG";
static const char LOG_LEVEL_DEBUG[] PROGMEM = "DEBUG";
static const char LOG_LEVEL_VERBOSE[] PROGMEM = "VERBOSE";
static const char LOG_LEVEL_VERY_VERBOSE[] PROGMEM = "VERY_VERBOSE";

static const LogString *const LOG_LEVELS[] = {
    reinterpret_cast<const LogString *>(LOG_LEVEL_NONE),    reinterpret_cast<const LogString *>(LOG_LEVEL_ERROR),
    reinterpret_cast<const LogString *>(LOG_LEVEL_WARN),    reinterpret_cast<const LogString *>(LOG_LEVEL_INFO),
    reinterpret_cast<const LogString *>(LOG_LEVEL_CONFIG),  reinterpret_cast<const LogString *>(LOG_LEVEL_DEBUG),
    reinterpret_cast<const LogString *>(LOG_LEVEL_VERBOSE), reinterpret_cast<const LogString *>(LOG_LEVEL_VERY_VERBOSE),
};
#else
static const char *const LOG_LEVELS[] = {"NONE", "ERROR", "WARN", "INFO", "CONFIG", "DEBUG", "VERBOSE", "VERY_VERBOSE"};
#endif

void Logger::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Logger:\n"
                "  Max Level: %s\n"
                "  Initial Level: %s",
                LOG_STR_ARG(LOG_LEVELS[ESPHOME_LOG_LEVEL]), LOG_STR_ARG(LOG_LEVELS[this->current_level_]));
#ifndef USE_HOST
  ESP_LOGCONFIG(TAG,
                "  Log Baud Rate: %" PRIu32 "\n"
                "  Hardware UART: %s",
                this->baud_rate_, LOG_STR_ARG(get_uart_selection_()));
#endif
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  if (this->log_buffer_) {
    ESP_LOGCONFIG(TAG, "  Task Log Buffer Size: %u", this->log_buffer_->size());
  }
#endif

#ifdef USE_LOGGER_RUNTIME_TAG_LEVELS
  for (auto &it : this->log_levels_) {
    ESP_LOGCONFIG(TAG, "  Level for '%s': %s", it.first, LOG_STR_ARG(LOG_LEVELS[it.second]));
  }
#endif
}

void Logger::set_log_level(uint8_t level) {
  if (level > ESPHOME_LOG_LEVEL) {
    level = ESPHOME_LOG_LEVEL;
    ESP_LOGW(TAG, "Cannot set log level higher than pre-compiled %s", LOG_STR_ARG(LOG_LEVELS[ESPHOME_LOG_LEVEL]));
  }
  this->current_level_ = level;
#ifdef USE_LOGGER_LEVEL_LISTENERS
  for (auto *listener : this->level_listeners_)
    listener->on_log_level_change(level);
#endif
}

Logger *global_logger = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::logger
