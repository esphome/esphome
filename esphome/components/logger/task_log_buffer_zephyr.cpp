#ifdef USE_ZEPHYR

#include "task_log_buffer_zephyr.h"

namespace esphome::logger {

__thread bool non_main_task_recursion_guard;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_ESPHOME_TASK_LOG_BUFFER

static inline uint32_t total_size_in_32bit_words(uint16_t text_length) {
  // Calculate total size in 32-bit words needed (header + text length + null terminator + 3(4 bytes alignment)
  return (sizeof(TaskLogBuffer::LogMessage) + text_length + 1 + 3) / sizeof(uint32_t);
}

static inline uint32_t get_wlen(const mpsc_pbuf_generic *item) {
  return total_size_in_32bit_words(reinterpret_cast<const TaskLogBuffer::LogMessage *>(item)->text_length);
}

TaskLogBuffer::TaskLogBuffer() {
  // Storage is a member array (embedded in Logger), no heap allocation needed
  this->mpsc_config_.buf = this->buf_storage_;
  this->mpsc_config_.size = BUF_WORD_COUNT;
  this->mpsc_config_.flags = MPSC_PBUF_MODE_OVERWRITE;
  this->mpsc_config_.get_wlen = get_wlen;

  mpsc_pbuf_init(&this->log_buffer_, &this->mpsc_config_);
}

bool TaskLogBuffer::send_message_thread_safe(uint8_t level, const char *tag, uint16_t line, const char *thread_name,
                                             const char *format, va_list args) {
  // Measure on a copy; the caller's va_list formats the text later
  va_list args_copy;
  va_copy(args_copy, args);
  int ret = task_log_cap_length(vsnprintf(nullptr, 0, format, args_copy));
  va_end(args_copy);
  if (ret <= 0) {
    return false;  // Formatting error or empty message
  }
  uint16_t text_length = ret;
  size_t total_size = total_size_in_32bit_words(text_length);
  auto *msg = reinterpret_cast<LogMessage *>(mpsc_pbuf_alloc(&this->log_buffer_, total_size, K_NO_WAIT));
  if (msg == nullptr) {
    return false;
  }
  fill_task_log_header(msg, level, tag, line, thread_name);

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

  text_length = trim_trailing_newlines(message->text_data(), message->text_length);

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
