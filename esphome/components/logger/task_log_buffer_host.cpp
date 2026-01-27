#ifdef USE_HOST

#include "task_log_buffer_host.h"

#ifdef USE_ESPHOME_TASK_LOG_BUFFER

#include "esphome/core/log.h"
#include <algorithm>
#include <cstdio>

namespace esphome::logger {

TaskLogBufferHost::TaskLogBufferHost(size_t slot_count) : slot_count_(slot_count) {
  // Allocate message slots
  this->slots_ = std::make_unique<LogMessage[]>(slot_count);
}

TaskLogBufferHost::~TaskLogBufferHost() {
  // unique_ptr handles cleanup automatically
}

int TaskLogBufferHost::acquire_write_slot_() {
  // Try to reserve a slot using compare-and-swap
  size_t current_reserve = this->reserve_index_.load(std::memory_order_relaxed);

  while (true) {
    // Calculate next index (with wrap-around)
    size_t next_reserve = (current_reserve + 1) % this->slot_count_;

    // Check if buffer would be full
    // Buffer is full when next write position equals read position
    size_t current_read = this->read_index_.load(std::memory_order_acquire);
    if (next_reserve == current_read) {
      return -1;  // Buffer full
    }

    // Try to claim this slot
    if (this->reserve_index_.compare_exchange_weak(current_reserve, next_reserve, std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
      return static_cast<int>(current_reserve);
    }
    // If CAS failed, current_reserve was updated, retry with new value
  }
}

void TaskLogBufferHost::commit_write_slot_(int slot_index) {
  // Mark the slot as ready for reading
  this->slots_[slot_index].ready.store(true, std::memory_order_release);

  // Try to advance the write_index if we're the next expected commit
  // This ensures messages are read in order
  size_t expected = slot_index;
  size_t next = (slot_index + 1) % this->slot_count_;

  // We only advance write_index if this slot is the next one expected
  // This handles out-of-order commits correctly
  while (true) {
    if (!this->write_index_.compare_exchange_weak(expected, next, std::memory_order_release,
                                                  std::memory_order_relaxed)) {
      // Someone else advanced it or we're not next in line, that's fine
      break;
    }

    // Successfully advanced, check if next slot is also ready
    expected = next;
    next = (next + 1) % this->slot_count_;
    if (!this->slots_[expected].ready.load(std::memory_order_acquire)) {
      break;
    }
  }
}

bool TaskLogBufferHost::send_message_thread_safe(uint8_t level, const char *tag, uint16_t line, const char *format,
                                                 va_list args) {
  // Acquire a slot
  int slot_index = this->acquire_write_slot_();
  if (slot_index < 0) {
    return false;  // Buffer full
  }

  LogMessage &msg = this->slots_[slot_index];

  // Fill in the message header
  msg.level = level;
  msg.tag = tag;
  msg.line = line;

  // Get thread name using pthread
  char thread_name_buf[LogMessage::MAX_THREAD_NAME_SIZE];
  // pthread_getname_np works the same on Linux and macOS
  if (pthread_getname_np(pthread_self(), thread_name_buf, sizeof(thread_name_buf)) == 0) {
    strncpy(msg.thread_name, thread_name_buf, sizeof(msg.thread_name) - 1);
    msg.thread_name[sizeof(msg.thread_name) - 1] = '\0';
  } else {
    msg.thread_name[0] = '\0';
  }

  // Format the message text
  int ret = vsnprintf(msg.text, sizeof(msg.text), format, args);
  if (ret < 0) {
    // Formatting error - still commit the slot but with empty text
    msg.text[0] = '\0';
    msg.text_length = 0;
  } else {
    msg.text_length = static_cast<uint16_t>(std::min(static_cast<size_t>(ret), sizeof(msg.text) - 1));
  }

  // Remove trailing newlines
  while (msg.text_length > 0 && msg.text[msg.text_length - 1] == '\n') {
    msg.text_length--;
  }
  msg.text[msg.text_length] = '\0';

  // Commit the slot
  this->commit_write_slot_(slot_index);

  return true;
}

bool TaskLogBufferHost::get_message_main_loop(LogMessage **message) {
  if (message == nullptr) {
    return false;
  }

  size_t current_read = this->read_index_.load(std::memory_order_relaxed);
  size_t current_write = this->write_index_.load(std::memory_order_acquire);

  // Check if buffer is empty
  if (current_read == current_write) {
    return false;
  }

  // Check if the slot is ready (should always be true if write_index advanced)
  LogMessage &msg = this->slots_[current_read];
  if (!msg.ready.load(std::memory_order_acquire)) {
    return false;
  }

  *message = &msg;
  return true;
}

void TaskLogBufferHost::release_message_main_loop() {
  size_t current_read = this->read_index_.load(std::memory_order_relaxed);

  // Clear the ready flag
  this->slots_[current_read].ready.store(false, std::memory_order_release);

  // Advance read index
  size_t next_read = (current_read + 1) % this->slot_count_;
  this->read_index_.store(next_read, std::memory_order_release);
}

}  // namespace esphome::logger

#endif  // USE_ESPHOME_TASK_LOG_BUFFER
#endif  // USE_HOST
