#ifdef USE_ESP32

#include "task_log_buffer_esp32.h"
#include "esphome/core/log.h"

#ifdef USE_ESPHOME_TASK_LOG_BUFFER

namespace esphome::logger {

TaskLogBuffer::TaskLogBuffer() {
  // Create a static ring buffer with RINGBUF_TYPE_NOSPLIT for message integrity
  // Storage is a member array (embedded in Logger), no heap allocation needed
  xRingbufferCreateStatic(sizeof(this->storage_), RINGBUF_TYPE_NOSPLIT, this->storage_, &this->structure_);
}

bool TaskLogBuffer::borrow_message_main_loop(LogMessage *&message, uint16_t &text_length) {
  if (this->current_token_) {
    return false;
  }

  size_t item_size = 0;
  void *received_item = xRingbufferReceive(this->handle_(), &item_size, 0);
  if (received_item == nullptr) {
    return false;
  }

  LogMessage *msg = static_cast<LogMessage *>(received_item);
  message = msg;
  text_length = msg->text_length;
  this->current_token_ = received_item;

  return true;
}

void TaskLogBuffer::release_message_main_loop() {
  if (this->current_token_ == nullptr) {
    return;
  }
  vRingbufferReturnItem(this->handle_(), this->current_token_);
  this->current_token_ = nullptr;
  // Update counter to mark all messages as processed
  last_processed_counter_ = message_counter_.load(std::memory_order_relaxed);
}

bool TaskLogBuffer::send_message_thread_safe(uint8_t level, const char *tag, uint16_t line, const char *thread_name,
                                             const char *format, va_list args) {
  int ret = task_log_text_length(format, args);
  if (ret <= 0) {
    return false;  // Formatting error or empty message
  }
  uint16_t text_length = ret;

  // Calculate total size needed (header + text length + null terminator)
  size_t total_size = sizeof(LogMessage) + text_length + 1;

  // Acquire memory directly from the ring buffer
  void *acquired_memory = nullptr;
  BaseType_t result = xRingbufferSendAcquire(this->handle_(), &acquired_memory, total_size, 0);

  if (result != pdTRUE || acquired_memory == nullptr) {
    return false;  // Failed to acquire memory
  }

  LogMessage *msg = static_cast<LogMessage *>(acquired_memory);
  fill_task_log_header(msg, level, tag, line, thread_name);

  // Format the message text directly into the acquired memory
  char *text_area = msg->text_data();
  ret = vsnprintf(text_area, text_length + 1, format, args);

  // Handle unexpected formatting error
  if (ret <= 0) {
    vRingbufferReturnItem(this->handle_(), acquired_memory);
    return false;
  }

  msg->text_length = trim_trailing_newlines(text_area, text_length);
  // Complete the send operation with the acquired memory
  result = xRingbufferSendComplete(this->handle_(), acquired_memory);

  if (result != pdTRUE) {
    return false;  // Failed to complete the message send
  }

  // Message sent successfully, increment the counter
  message_counter_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

}  // namespace esphome::logger

#endif  // USE_ESPHOME_TASK_LOG_BUFFER
#endif  // USE_ESP32
