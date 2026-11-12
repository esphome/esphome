#pragma once

#ifdef USE_LIBRETINY

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESPHOME_TASK_LOG_BUFFER
#include <cstdarg>
#include <cstddef>
#include <cstring>
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "task_log_message.h"

namespace esphome::logger {

/**
 * @brief Task log buffer for LibreTiny platform using mutex-protected circular buffer.
 *
 * Why This Is Critical:
 * Without thread-safe logging, when a non-main task logs a message, it would directly
 * call the logger which builds a protobuf message in a shared buffer. If this happens
 * while the main loop is also using that buffer (e.g., sending API responses), the
 * buffer gets corrupted, sending garbage to all connected API clients and breaking
 * their connections. This buffer ensures log messages from other tasks are queued
 * safely and processed only from the main loop.
 *
 * Threading Model: Multi-Producer Single-Consumer (MPSC)
 * - Multiple FreeRTOS tasks can safely call send_message_thread_safe() concurrently
 * - Only the main loop task calls borrow_message_main_loop() and release_message_main_loop()
 *
 * This uses a simple circular buffer protected by a FreeRTOS mutex. Unlike ESP32,
 * LibreTiny lacks hardware atomic support (ARM968E-S has no LDREX/STREX), so we use
 * a volatile counter for fast has_messages() checks instead of atomics.
 *
 * Design:
 * - Variable-size messages with header + text stored contiguously (NOSPLIT style)
 * - FreeRTOS mutex protects all buffer operations
 * - Volatile counter enables fast has_messages() without lock overhead
 * - If message doesn't fit at end, padding is added and message wraps to start
 */
class TaskLogBuffer {
 public:
  using LogMessage = TaskLogMessage;

  // Padding marker level to indicate wrap-around point (stored in LogMessage.level field)
  // Valid log levels are 0-7, so 0xFF cannot be a real message
  static constexpr uint8_t PADDING_MARKER_LEVEL = 0xFF;

  TaskLogBuffer();
  // No destructor: Logger is never destroyed

  // NOT thread-safe - borrow a message from the buffer, only call from main loop
  bool borrow_message_main_loop(LogMessage *&message, uint16_t &text_length);

  // NOT thread-safe - release a message buffer, only call from main loop
  void release_message_main_loop();

  // Thread-safe - send a message to the buffer from any thread
  bool send_message_thread_safe(uint8_t level, const char *tag, uint16_t line, const char *thread_name,
                                const char *format, va_list args);

  // Fast check using volatile counter - no lock needed
  // Worst case: miss a message for one loop iteration (~8ms at 7000 loops/min)
  inline bool HOT has_messages() const { return this->message_count_ != 0; }

  // Get the total buffer size in bytes
  static constexpr size_t size() { return ESPHOME_TASK_LOG_BUFFER_SIZE; }

 private:
  // Calculate total size needed for a message (header + text + null terminator)
  static inline size_t message_total_size(size_t text_length) { return sizeof(LogMessage) + text_length + 1; }

  // Calculate available contiguous space at write position
  size_t available_contiguous_space_() const;

  uint8_t storage_[ESPHOME_TASK_LOG_BUFFER_SIZE];  // Embedded in Logger (no separate heap allocation)
  size_t head_{0};                                 // Write position
  size_t tail_{0};                                 // Read position

  SemaphoreHandle_t mutex_{nullptr};    // FreeRTOS mutex for thread safety
  volatile uint16_t message_count_{0};  // Fast check counter (dirty read OK)
  size_t current_message_size_{0};      // Size of currently borrowed message
};

}  // namespace esphome::logger

#endif  // USE_ESPHOME_TASK_LOG_BUFFER
#endif  // USE_LIBRETINY
