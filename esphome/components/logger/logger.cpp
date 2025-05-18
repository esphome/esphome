#include "logger.h"
#include <cinttypes>
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
#include <memory>  // For unique_ptr
#endif

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace logger {

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
void HOT Logger::log_vprintf_(int level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
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
  message_sent = this->log_buffer_->send_message_thread_safe(static_cast<uint8_t>(level), tag,
                                                             static_cast<uint16_t>(line), current_task, format, args);
#endif  // USE_ESPHOME_TASK_LOG_BUFFER

  // Emergency console logging for non-main tasks when ring buffer is full or disabled
  // This is a fallback mechanism to ensure critical log messages are visible
  // Note: This may cause interleaved/corrupted console output if multiple tasks
  // log simultaneously, but it's better than losing important messages entirely
  if (!message_sent && this->baud_rate_ > 0) {  // If logging is enabled, write to console
    // Maximum size for console log messages (includes null terminator)
    static const size_t MAX_CONSOLE_LOG_MSG_SIZE = 144;
    char console_buffer[MAX_CONSOLE_LOG_MSG_SIZE];  // MUST be stack allocated for thread safety
    int buffer_at = 0;                              // Initialize buffer position
    this->format_log_to_buffer_with_terminator_(level, tag, line, format, args, console_buffer, &buffer_at,
                                                MAX_CONSOLE_LOG_MSG_SIZE);
    this->write_msg_(console_buffer);
  }

  // Reset the recursion guard for this task
  this->reset_task_log_recursion_(is_main_task);
}
#else
// Implementation for all other platforms
void HOT Logger::log_vprintf_(int level, const char *tag, int line, const char *format, va_list args) {  // NOLINT
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
void Logger::log_vprintf_(int level, const char *tag, int line, const __FlashStringHelper *format,
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
  uint32_t msg_start = this->tx_buffer_at_;
  this->format_log_to_buffer_with_terminator_(level, tag, line, this->tx_buffer_, args, this->tx_buffer_,
                                              &this->tx_buffer_at_, this->tx_buffer_size_);

  // Write to console and send callback starting at the msg_start
  if (this->baud_rate_ > 0) {
    this->write_msg_(this->tx_buffer_ + msg_start);
  }
  this->call_log_callbacks_(level, tag, this->tx_buffer_ + msg_start);

  global_recursion_guard_ = false;
}
#endif  // USE_STORE_LOG_STR_IN_FLASH

inline int Logger::level_for(const char *tag) {
  auto it = this->log_levels_.find(tag);
  if (it != this->log_levels_.end())
    return it->second;
  return this->current_level_;
}

void HOT Logger::call_log_callbacks_(int level, const char *tag, const char *msg) {
#ifdef USE_ESP32
  // Suppress network-logging if memory constrained
  // In some configurations (eg BLE enabled) there may be some transient
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
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
void Logger::init_log_buffer(size_t total_buffer_size) {
  this->log_buffer_ = esphome::make_unique<logger::TaskLogBuffer>(total_buffer_size);
}
#endif

#if defined(USE_LOGGER_USB_CDC) || defined(USE_ESP32)
void Logger::loop() {
#if defined(USE_LOGGER_USB_CDC) && defined(USE_ARDUINO)
  if (this->uart_ == UART_SELECTION_USB_CDC) {
    static bool opened = false;
    if (opened == Serial) {
      return;
    }
    if (false == opened) {
      App.schedule_dump_config();
    }
    opened = !opened;
  }
#endif

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
      this->call_log_callbacks_(message->level, message->tag, this->tx_buffer_);
      // At this point all the data we need from message has been transferred to the tx_buffer
      // so we can release the message to allow other tasks to use it as soon as possible.
      this->log_buffer_->release_message_main_loop(received_token);

      // Write to console from the main loop to prevent corruption from concurrent writes
      // This ensures all log messages appear on the console in a clean, serialized manner
      // Note: Messages may appear slightly out of order due to async processing, but
      // this is preferred over corrupted/interleaved console output
      if (this->baud_rate_ > 0) {
        this->write_msg_(this->tx_buffer_);
      }
    }
  }
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
static const char *const LOG_LEVELS[] = {"NONE", "ERROR", "WARN", "INFO", "CONFIG", "DEBUG", "VERBOSE", "VERY_VERBOSE"};

void Logger::dump_config() {
  ESP_LOGCONFIG(TAG, "Logger:");
  ESP_LOGCONFIG(TAG, "  Max Level: %s", LOG_LEVELS[ESPHOME_LOG_LEVEL]);
  ESP_LOGCONFIG(TAG, "  Initial Level: %s", LOG_LEVELS[this->current_level_]);
#ifndef USE_HOST
  ESP_LOGCONFIG(TAG, "  Log Baud Rate: %" PRIu32, this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Hardware UART: %s", get_uart_selection_());
#endif
#ifdef USE_ESPHOME_TASK_LOG_BUFFER
  if (this->log_buffer_) {
    ESP_LOGCONFIG(TAG, "  Task Log Buffer Size: %u", this->log_buffer_->size());
  }
#endif

  for (auto &it : this->log_levels_) {
    ESP_LOGCONFIG(TAG, "  Level for '%s': %s", it.first.c_str(), LOG_LEVELS[it.second]);
  }
}

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
