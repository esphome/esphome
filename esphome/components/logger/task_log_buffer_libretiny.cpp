#ifdef USE_LIBRETINY

#include "task_log_buffer_libretiny.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESPHOME_TASK_LOG_BUFFER

namespace esphome::logger {

TaskLogBufferLibreTiny::TaskLogBufferLibreTiny(size_t total_buffer_size) {
  this->size_ = total_buffer_size;
  // Allocate memory for the circular buffer using ESPHome's RAM allocator
  RAMAllocator<uint8_t> allocator;
  this->storage_ = allocator.allocate(this->size_);
  // Create mutex for thread-safe access
  this->mutex_ = xSemaphoreCreateMutex();
}

TaskLogBufferLibreTiny::~TaskLogBufferLibreTiny() {
  if (this->mutex_ != nullptr) {
    vSemaphoreDelete(this->mutex_);
    this->mutex_ = nullptr;
  }
  if (this->storage_ != nullptr) {
    RAMAllocator<uint8_t> allocator;
    allocator.deallocate(this->storage_, this->size_);
    this->storage_ = nullptr;
  }
}

size_t TaskLogBufferLibreTiny::available_contiguous_space() const {
  if (this->head_ >= this->tail_) {
    // head is ahead of or equal to tail
    // Available space is from head to end, plus from start to tail
    // But for contiguous, just from head to end (minus 1 to avoid head==tail ambiguity)
    size_t space_to_end = this->size_ - this->head_;
    if (this->tail_ == 0) {
      // Can't use the last byte or head would equal tail
      return space_to_end > 0 ? space_to_end - 1 : 0;
    }
    return space_to_end;
  } else {
    // tail is ahead of head
    // Available contiguous space is from head to tail - 1
    return this->tail_ - this->head_ - 1;
  }
}

bool TaskLogBufferLibreTiny::borrow_message_main_loop(LogMessage **message, const char **text) {
  if (message == nullptr || text == nullptr) {
    return false;
  }

  // Check if buffer was initialized successfully
  if (this->mutex_ == nullptr || this->storage_ == nullptr) {
    return false;
  }

  // Try to take mutex without blocking - if busy, we'll get messages next loop iteration
  if (xSemaphoreTake(this->mutex_, 0) != pdTRUE) {
    return false;
  }

  if (this->head_ == this->tail_) {
    xSemaphoreGive(this->mutex_);
    return false;
  }

  // Read message header from tail
  LogMessage *msg = reinterpret_cast<LogMessage *>(this->storage_ + this->tail_);

  // Check for padding marker (indicates wrap-around)
  // We check the level field since valid levels are 0-7, and 0xFF indicates padding
  if (msg->level == PADDING_MARKER_LEVEL) {
    // Skip to start of buffer and re-read
    this->tail_ = 0;
    msg = reinterpret_cast<LogMessage *>(this->storage_);
  }
  *message = msg;
  *text = msg->text_data();
  this->current_message_size_ = message_total_size(msg->text_length);

  // Keep mutex held until release_message_main_loop()
  return true;
}

void TaskLogBufferLibreTiny::release_message_main_loop() {
  // Advance tail past the current message
  this->tail_ += this->current_message_size_;

  // Handle wrap-around if we've reached the end
  if (this->tail_ >= this->size_) {
    this->tail_ = 0;
  }

  this->message_count_--;
  this->current_message_size_ = 0;

  xSemaphoreGive(this->mutex_);
}

bool TaskLogBufferLibreTiny::send_message_thread_safe(uint8_t level, const char *tag, uint16_t line,
                                                      TaskHandle_t task_handle, const char *format, va_list args) {
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
  size_t total_size = message_total_size(text_length);

  // Check if buffer was initialized successfully
  if (this->mutex_ == nullptr || this->storage_ == nullptr) {
    return false;  // Buffer not initialized, fall back to direct output
  }

  // Try to acquire mutex without blocking - don't block logging tasks
  if (xSemaphoreTake(this->mutex_, 0) != pdTRUE) {
    return false;  // Mutex busy, fall back to direct output
  }

  // Check if we have enough contiguous space
  size_t contiguous = this->available_contiguous_space();

  if (contiguous < total_size) {
    // Not enough contiguous space at end
    // Check if we can wrap around
    size_t space_at_start = (this->head_ >= this->tail_) ? this->tail_ : 0;
    if (space_at_start > 0) {
      space_at_start--;  // Leave 1 byte gap to distinguish full from empty
    }

    // Need at least enough space to safely write padding marker (level field is at end of struct)
    constexpr size_t PADDING_MARKER_MIN_SPACE = offsetof(LogMessage, level) + 1;

    if (space_at_start >= total_size && this->head_ > 0 && contiguous >= PADDING_MARKER_MIN_SPACE) {
      // Add padding marker (set level field to indicate this is padding, not a real message)
      LogMessage *padding = reinterpret_cast<LogMessage *>(this->storage_ + this->head_);
      padding->level = PADDING_MARKER_LEVEL;
      this->head_ = 0;
    } else {
      // Not enough space anywhere, or can't safely write padding marker
      xSemaphoreGive(this->mutex_);
      return false;
    }
  }

  // Write message header
  LogMessage *msg = reinterpret_cast<LogMessage *>(this->storage_ + this->head_);
  msg->level = level;
  msg->tag = tag;
  msg->line = line;

  // Store the thread name now to avoid crashes if task is deleted before processing
  const char *thread_name = pcTaskGetTaskName(task_handle);
  if (thread_name != nullptr) {
    strncpy(msg->thread_name, thread_name, sizeof(msg->thread_name) - 1);
    msg->thread_name[sizeof(msg->thread_name) - 1] = '\0';
  } else {
    msg->thread_name[0] = '\0';
  }

  // Format the message text directly into the buffer
  char *text_area = msg->text_data();
  ret = vsnprintf(text_area, text_length + 1, format, args);

  if (ret <= 0) {
    xSemaphoreGive(this->mutex_);
    return false;
  }

  // Remove trailing newlines
  while (text_length > 0 && text_area[text_length - 1] == '\n') {
    text_length--;
  }

  msg->text_length = text_length;

  // Advance head
  this->head_ += total_size;

  // Handle wrap-around (shouldn't happen due to contiguous space check, but be safe)
  if (this->head_ >= this->size_) {
    this->head_ = 0;
  }

  this->message_count_++;

  xSemaphoreGive(this->mutex_);
  return true;
}

}  // namespace esphome::logger

#endif  // USE_ESPHOME_TASK_LOG_BUFFER
#endif  // USE_LIBRETINY
