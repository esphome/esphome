#ifdef USE_ZEPHYR

#include "task_log_buffer_zephyr.h"

#ifdef USE_ESPHOME_TASK_LOG_BUFFER

namespace esphome::logger {

__thread bool non_main_task_recursion_guard_;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static inline uint32_t get_wlen(const mpsc_pbuf_generic *item) {
  auto *msg = reinterpret_cast<const TaskLogBufferZephyr::LogMessage *>(item);
  // Calculate total size in 32-bit words needed (header + text length + null terminator + 3(4 bytes alignment)
  return (sizeof(TaskLogBufferZephyr::LogMessage) + msg->text_length + 1 + 3) / sizeof(uint32_t);
}

TaskLogBufferZephyr::TaskLogBufferZephyr(size_t total_buffer_size) {
  // alignment to 4 bytes
  total_buffer_size = (total_buffer_size + 3) / sizeof(uint32_t);
  this->mpsc_config_.buf = new uint32_t[total_buffer_size];
  this->mpsc_config_.size = total_buffer_size;
  this->mpsc_config_.flags = MPSC_PBUF_MODE_OVERWRITE;
  this->mpsc_config_.get_wlen = get_wlen,

  mpsc_pbuf_init(&this->log_buffer_, &this->mpsc_config_);
}

TaskLogBufferZephyr::~TaskLogBufferZephyr() { delete[] this->mpsc_config_.buf; }

bool TaskLogBufferZephyr::send_message_thread_safe(uint8_t level, const char *tag, uint16_t line, void *task_handle,
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
  // Calculate total size in 32-bit words needed (header + text length + null terminator + 3(4 bytes alignment)
  size_t total_size = (sizeof(LogMessage) + text_length + 1 + 3) / sizeof(uint32_t);
  auto *msg = reinterpret_cast<LogMessage *>(mpsc_pbuf_alloc(&this->log_buffer_, total_size, K_NO_WAIT));
  if (nullptr == msg) {
    return false;
  }
  msg->level = level;
  msg->tag = tag;
  msg->line = line;
  const char *thread_name = k_thread_name_get(static_cast<k_tid_t>(task_handle));
  if (thread_name) {
    strncpy(msg->thread_name, thread_name, sizeof(msg->thread_name) - 1);
  } else {
    std::snprintf(msg->thread_name, sizeof(msg->thread_name), "%p", task_handle);
  }

  // Format the message text directly into the acquired memory
  // We add 1 to text_length to ensure space for null terminator during formatting
  char *text_area = msg->text_data();
  ret = vsnprintf(text_area, text_length + 1, format, args);

  // Handle unexpected formatting error
  if (ret <= 0) {
    // this shall not happened vsnprintf was called already once
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

bool TaskLogBufferZephyr::borrow_message_main_loop(LogMessage **message, const char **text) {
  if (this->current_token_) {
    return false;
  }

  this->current_token_ = mpsc_pbuf_claim(&this->log_buffer_);

  if (nullptr == this->current_token_) {
    return false;
  }

  // we claimed buffer alraedy const_cast is safe here
  *message = const_cast<LogMessage *>(reinterpret_cast<const LogMessage *>(this->current_token_));

  *text = (*message)->text_data();

  // Remove trailing newlines
  while ((*message)->text_length > 0 && (*text)[(*message)->text_length - 1] == '\n') {
    (*message)->text_length--;
  }

  return true;
}

void TaskLogBufferZephyr::release_message_main_loop() {
  if (this->current_token_ == nullptr) {
    return;
  }
  mpsc_pbuf_free(&this->log_buffer_, this->current_token_);
  this->current_token_ = nullptr;
}
}  // namespace esphome::logger

#endif
#endif
