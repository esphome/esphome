#ifdef USE_ZEPHYR

#include "task_log_buffer_zephyr.h"

namespace esphome::logger {

__thread bool non_main_task_recursion_guard_;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_ESPHOME_TASK_LOG_BUFFER

static inline uint32_t total_size_in_32bit_words(uint16_t text_length) {
  // Calculate total size in 32-bit words needed (header + text length + null terminator + 3(4 bytes alignment)
  return (sizeof(TaskLogBuffer::LogMessage) + text_length + 1 + 3) / sizeof(uint32_t);
}

static inline uint32_t get_wlen(const mpsc_pbuf_generic *item) {
  return total_size_in_32bit_words(reinterpret_cast<const TaskLogBuffer::LogMessage *>(item)->text_length);
}

TaskLogBuffer::TaskLogBuffer(size_t total_buffer_size) {
  // alignment to 4 bytes
  total_buffer_size = (total_buffer_size + 3) / sizeof(uint32_t);
  this->mpsc_config_.buf = new uint32_t[total_buffer_size];
  this->mpsc_config_.size = total_buffer_size;
  this->mpsc_config_.flags = MPSC_PBUF_MODE_OVERWRITE;
  this->mpsc_config_.get_wlen = get_wlen,

  mpsc_pbuf_init(&this->log_buffer_, &this->mpsc_config_);
}

TaskLogBuffer::~TaskLogBuffer() { delete[] this->mpsc_config_.buf; }

bool TaskLogBuffer::send_message_thread_safe(uint8_t level, const char *tag, uint16_t line, const char *thread_name,
                                             const char *format, va_list args) {
  // First, calculate the exact length needed using a null buffer (no actual writing)
  va_list args_copy;
  va_copy(args_copy, args);
  int ret = vsnprintf(nullptr, 0, format, args_copy);
  va_end(args_copy);

  if (ret <= 0) {
    return false;  // Formatting error or empty message
  }

  // Calculate actual text length (capped to maximum size)
  static constexpr size_t MAX_TEXT_SIZE = 255;
  size_t text_length = (static_cast<size_t>(ret) > MAX_TEXT_SIZE) ? MAX_TEXT_SIZE : ret;
  size_t total_size = total_size_in_32bit_words(text_length);
  auto *msg = reinterpret_cast<LogMessage *>(mpsc_pbuf_alloc(&this->log_buffer_, total_size, K_NO_WAIT));
  if (msg == nullptr) {
    return false;
  }
  msg->level = level;
  msg->tag = tag;
  msg->line = line;
  strncpy(msg->thread_name, thread_name, sizeof(msg->thread_name) - 1);
  msg->thread_name[sizeof(msg->thread_name) - 1] = '\0';  // Ensure null termination

  // Format the message text directly into the acquired memory
  // We add 1 to text_length to ensure space for null terminator during formatting
  char *text_area = msg->text_data();
  ret = vsnprintf(text_area, text_length + 1, format, args);

  // Handle unexpected formatting error (ret < 0 is encoding error; ret == 0 is valid empty output)
  if (ret < 0) {
    // this should not happen, vsnprintf was called already once
    // fill with '\n' to not call mpsc_pbuf_free from producer
    // it will be trimmed anyway
    for (size_t i = 0; i < text_length; ++i) {
      text_area[i] = '\n';
    }
    text_area[text_length] = 0;
    // do not return false to free the buffer from main thread
  }

  msg->text_length = text_length;

  mpsc_pbuf_commit(&this->log_buffer_, reinterpret_cast<mpsc_pbuf_generic *>(msg));
  return true;
}

bool TaskLogBuffer::borrow_message_main_loop(LogMessage *&message, uint16_t &text_length) {
  if (this->current_token_) {
    return false;
  }

  this->current_token_ = mpsc_pbuf_claim(&this->log_buffer_);

  if (this->current_token_ == nullptr) {
    return false;
  }

  // we claimed buffer already, const_cast is safe here
  message = const_cast<LogMessage *>(reinterpret_cast<const LogMessage *>(this->current_token_));

  text_length = message->text_length;
  // Remove trailing newlines
  while (text_length > 0 && message->text_data()[text_length - 1] == '\n') {
    text_length--;
  }

  return true;
}

void TaskLogBuffer::release_message_main_loop() {
  if (this->current_token_ == nullptr) {
    return;
  }
  mpsc_pbuf_free(&this->log_buffer_, this->current_token_);
  this->current_token_ = nullptr;
}
#endif  // USE_ESPHOME_TASK_LOG_BUFFER

}  // namespace esphome::logger

#endif  // USE_ZEPHYR
