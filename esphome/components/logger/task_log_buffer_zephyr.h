#pragma once

#ifdef USE_ZEPHYR

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include <zephyr/sys/mpsc_pbuf.h>

namespace esphome::logger {

// "0x" + 2 hex digits per byte + '\0'
static constexpr size_t MAX_POINTER_REPRESENTATION = 2 + sizeof(void *) * 2 + 1;

extern __thread bool non_main_task_recursion_guard_;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_ESPHOME_TASK_LOG_BUFFER

class TaskLogBuffer {
 public:
  // Structure for a log message header (text data follows immediately after)
  struct LogMessage {
    MPSC_PBUF_HDR;  // this is only 2 bits but no more than 30 bits directly after
    uint16_t line;  // Source code line number
    uint8_t level;  // Log level (0-7)
#if defined(CONFIG_THREAD_NAME)
    char thread_name[CONFIG_THREAD_MAX_NAME_LEN];  // Store thread name directly (only used for non-main threads)
#else
    char thread_name[MAX_POINTER_REPRESENTATION];  // Store thread name directly (only used for non-main threads)
#endif
    const char *tag;       // We store the pointer, assuming tags are static
    uint16_t text_length;  // Length of the message text (up to ~64KB)

    // Methods for accessing message contents
    inline char *text_data() { return reinterpret_cast<char *>(this) + sizeof(LogMessage); }
  };
  TaskLogBuffer();
  ~TaskLogBuffer() = default;

  // Check if there are messages ready to be processed using an atomic counter for performance
  inline bool HOT has_messages() { return mpsc_pbuf_is_pending(&this->log_buffer_); }

  // Get the total buffer size in bytes
  static constexpr size_t size() { return BUF_WORD_COUNT * sizeof(uint32_t); }

  // NOT thread-safe - borrow a message from the ring buffer, only call from main loop
  bool borrow_message_main_loop(LogMessage *&message, uint16_t &text_length);

  // NOT thread-safe - release a message buffer and update the counter, only call from main loop
  void release_message_main_loop();

  // Thread-safe - send a message to the ring buffer from any thread
  bool send_message_thread_safe(uint8_t level, const char *tag, uint16_t line, const char *thread_name,
                                const char *format, va_list args);

 protected:
  // Round up byte size to 32-bit word count for mpsc_pbuf alignment requirement
  static constexpr size_t BUF_WORD_COUNT = (ESPHOME_TASK_LOG_BUFFER_SIZE + 3) / sizeof(uint32_t);
  uint32_t buf_storage_[BUF_WORD_COUNT];  // Embedded in Logger (no separate heap allocation)
  mpsc_pbuf_buffer_config mpsc_config_{};
  mpsc_pbuf_buffer log_buffer_{};
  const mpsc_pbuf_generic *current_token_{};
};

#endif  // USE_ESPHOME_TASK_LOG_BUFFER

}  // namespace esphome::logger

#endif  // USE_ZEPHYR
