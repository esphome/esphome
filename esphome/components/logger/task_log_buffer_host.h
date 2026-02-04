#pragma once

#ifdef USE_HOST

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESPHOME_TASK_LOG_BUFFER

#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstring>
#include <memory>
#include <pthread.h>

namespace esphome::logger {

/**
 * @brief Lock-free task log buffer for host platform.
 *
 * Threading Model: Multi-Producer Single-Consumer (MPSC)
 * - Multiple threads can safely call send_message_thread_safe() concurrently
 * - Only the main loop thread calls get_message_main_loop() and release_message_main_loop()
 *
 *   Producers (multiple threads)              Consumer (main loop only)
 *            │                                        │
 *            ▼                                        ▼
 *     acquire_write_slot_()                  get_message_main_loop()
 *       CAS on reserve_index_                  read write_index_
 *            │                                   check ready flag
 *            ▼                                        │
 *     write to slot (exclusive)                       ▼
 *            │                                  read slot data
 *            ▼                                        │
 *     commit_write_slot_()                            ▼
 *       set ready=true                       release_message_main_loop()
 *       advance write_index_                   set ready=false
 *                                              advance read_index_
 *
 * This implements a lock-free ring buffer for log messages on the host platform.
 * It uses atomic compare-and-swap (CAS) operations for thread-safe slot reservation
 * without requiring mutexes in the hot path.
 *
 * Design:
 * - Fixed number of pre-allocated message slots to avoid dynamic allocation
 * - Each slot contains a header and fixed-size text buffer
 * - Atomic CAS for slot reservation allows multiple producers without locks
 * - Single consumer (main loop) processes messages in order
 */
class TaskLogBufferHost {
 public:
  // Default number of message slots - host has plenty of memory
  static constexpr size_t DEFAULT_SLOT_COUNT = 64;

  // Structure for a log message (fixed size for lock-free operation)
  struct LogMessage {
    // Size constants
    static constexpr size_t MAX_THREAD_NAME_SIZE = 32;
    static constexpr size_t MAX_TEXT_SIZE = 512;

    const char *tag;                         // Pointer to static tag string
    char thread_name[MAX_THREAD_NAME_SIZE];  // Thread name (copied)
    char text[MAX_TEXT_SIZE + 1];            // Message text with null terminator
    uint16_t text_length;                    // Actual length of text
    uint16_t line;                           // Source line number
    uint8_t level;                           // Log level
    std::atomic<bool> ready;                 // Message is ready to be consumed

    LogMessage() : tag(nullptr), text_length(0), line(0), level(0), ready(false) {
      thread_name[0] = '\0';
      text[0] = '\0';
    }
  };

  /// Constructor that takes the number of message slots
  explicit TaskLogBufferHost(size_t slot_count);
  ~TaskLogBufferHost();

  // NOT thread-safe - get next message from buffer, only call from main loop
  // Returns true if a message was retrieved, false if buffer is empty
  bool get_message_main_loop(LogMessage **message);

  // NOT thread-safe - release the message after processing, only call from main loop
  void release_message_main_loop();

  // Thread-safe - send a message to the buffer from any thread
  // Returns true if message was queued, false if buffer is full
  bool send_message_thread_safe(uint8_t level, const char *tag, uint16_t line, const char *format, va_list args);

  // Check if there are messages ready to be processed
  inline bool HOT has_messages() const {
    return read_index_.load(std::memory_order_acquire) != write_index_.load(std::memory_order_acquire);
  }

  // Get the buffer size (number of slots)
  inline size_t size() const { return slot_count_; }

 private:
  // Acquire a slot for writing (thread-safe)
  // Returns slot index or -1 if buffer is full
  int acquire_write_slot_();

  // Commit a slot after writing (thread-safe)
  void commit_write_slot_(int slot_index);

  std::unique_ptr<LogMessage[]> slots_;  // Pre-allocated message slots
  size_t slot_count_;                    // Number of slots

  // Lock-free indices using atomics
  // - reserve_index_: Next slot to reserve (producers CAS this to claim slots)
  // - write_index_: Boundary of committed/ready slots (consumer reads up to this)
  // - read_index_: Next slot to read (only consumer modifies this)
  std::atomic<size_t> reserve_index_{0};  // Next slot to reserve for writing
  std::atomic<size_t> write_index_{0};    // Last committed slot boundary
  std::atomic<size_t> read_index_{0};     // Next slot to read from
};

}  // namespace esphome::logger

#endif  // USE_ESPHOME_TASK_LOG_BUFFER
#endif  // USE_HOST
