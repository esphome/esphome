
#include "task_log_buffer.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESPHOME_TASK_LOG_BUFFER

namespace esphome {
namespace logger {

TaskLogBuffer::TaskLogBuffer(size_t total_buffer_size) {
  // Store the buffer size
  this->size_ = total_buffer_size;
  // Allocate memory for the ring buffer using ESPHome's RAM allocator
  RAMAllocator<uint8_t> allocator;
  this->storage_ = allocator.allocate(this->size_);
  // Create a static ring buffer with RINGBUF_TYPE_NOSPLIT for message integrity
  this->ring_buffer_ = xRingbufferCreateStatic(this->size_, RINGBUF_TYPE_NOSPLIT, this->storage_, &this->structure_);
}

TaskLogBuffer::~TaskLogBuffer() {
  if (this->ring_buffer_ != nullptr) {
    // Delete the ring buffer
    vRingbufferDelete(this->ring_buffer_);
    this->ring_buffer_ = nullptr;

    // Free the allocated memory
    RAMAllocator<uint8_t> allocator;
    allocator.deallocate(this->storage_, this->size_);
    this->storage_ = nullptr;
  }
}

bool TaskLogBuffer::borrow_message_main_loop(LogMessage **message, const char **text, void **received_token) {
  if (message == nullptr || text == nullptr || received_token == nullptr) {
    return false;
  }

  size_t item_size = 0;
  void *received_item = xRingbufferReceive(ring_buffer_, &item_size, 0);
  if (received_item == nullptr) {
    return false;
  }

  LogMessage *msg = static_cast<LogMessage *>(received_item);
  *message = msg;
  *text = msg->text_data();
  *received_token = received_item;

  return true;
}

void TaskLogBuffer::release_message_main_loop(void *token) {
  if (token == nullptr) {
    return;
  }
  vRingbufferReturnItem(ring_buffer_, token);
  // Update counter to mark all messages as processed
  last_processed_counter_ = message_counter_.load(std::memory_order_relaxed);
}

bool TaskLogBuffer::send_message_thread_safe(uint8_t level, const char *tag, uint16_t line, TaskHandle_t task_handle,
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

  // Calculate total size needed (header + text length + null terminator)
  size_t total_size = sizeof(LogMessage) + text_length + 1;

  // Acquire memory directly from the ring buffer
  void *acquired_memory = nullptr;
  BaseType_t result = xRingbufferSendAcquire(ring_buffer_, &acquired_memory, total_size, 0);

  if (result != pdTRUE || acquired_memory == nullptr) {
    return false;  // Failed to acquire memory
  }

  // Set up the message header in the acquired memory
  LogMessage *msg = static_cast<LogMessage *>(acquired_memory);
  msg->level = level;
  msg->tag = tag;
  msg->line = line;

  // Store the thread name now instead of waiting until main loop processing
  // This avoids crashes if the task completes or is deleted between when this message
  // is enqueued and when it's processed by the main loop
  const char *thread_name = pcTaskGetName(task_handle);
  if (thread_name != nullptr) {
    strncpy(msg->thread_name, thread_name, sizeof(msg->thread_name) - 1);
    msg->thread_name[sizeof(msg->thread_name) - 1] = '\0';  // Ensure null termination
  } else {
    msg->thread_name[0] = '\0';  // Empty string if no thread name
  }

  // Format the message text directly into the acquired memory
  // We add 1 to text_length to ensure space for null terminator during formatting
  char *text_area = msg->text_data();
  ret = vsnprintf(text_area, text_length + 1, format, args);

  // Handle unexpected formatting error
  if (ret <= 0) {
    vRingbufferReturnItem(ring_buffer_, acquired_memory);
    return false;
  }

  // Remove trailing newlines
  while (text_length > 0 && text_area[text_length - 1] == '\n') {
    text_length--;
  }

  msg->text_length = text_length;
  // Complete the send operation with the acquired memory
  result = xRingbufferSendComplete(ring_buffer_, acquired_memory);

  if (result != pdTRUE) {
    return false;  // Failed to complete the message send
  }

  // Message sent successfully, increment the counter
  message_counter_.fetch_add(1, std::memory_order_relaxed);
  return true;
}

}  // namespace logger
}  // namespace esphome

#endif  // USE_ESPHOME_TASK_LOG_BUFFER
