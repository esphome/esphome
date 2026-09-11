#pragma once

#ifdef USE_ESP32

#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

#include <cinttypes>
#include <memory>

namespace esphome::ring_buffer {

class RingBuffer {
 public:
  ~RingBuffer();

  /**
   * @brief Reads from the ring buffer, waiting up to a specified number of ticks if necessary.
   *
   * Available bytes are read into the provided data pointer. If not enough bytes are available,
   * the function will wait up to `ticks_to_wait` FreeRTOS ticks before reading what is available.
   *
   * @param data Pointer to copy read data into
   * @param len Number of bytes to read
   * @param ticks_to_wait Maximum number of FreeRTOS ticks to wait (default: 0)
   * @return Number of bytes read
   */
  size_t read(void *data, size_t len, TickType_t ticks_to_wait = 0);

  /**
   * @brief Acquires a pointer into the ring buffer's internal storage without copying.
   *
   * The returned pointer is valid until receive_release() is called. Only one item
   * may be checked out at a time.
   *
   * @param[out] length Set to the number of bytes actually acquired (may be less than max_length at wrap boundary)
   * @param max_length Maximum number of bytes to acquire
   * @param ticks_to_wait Maximum number of FreeRTOS ticks to wait (default: 0)
   * @return Pointer into the ring buffer's internal storage, or nullptr if no data is available
   */
  void *receive_acquire(size_t &length, size_t max_length, TickType_t ticks_to_wait = 0);

  /**
   * @brief Releases a previously acquired ring buffer item.
   *
   * Must be called exactly once for each successful receive_acquire().
   *
   * @param item Pointer returned by receive_acquire()
   */
  void receive_release(void *item);

  /**
   * @brief Writes to the ring buffer, overwriting oldest data if necessary.
   *
   * The provided data is written to the ring buffer. If not enough space is available,
   * the function will overwrite the oldest data in the ring buffer.
   *
   * @param data Pointer to data for writing
   * @param len Number of bytes to write
   * @return Number of bytes written
   */
  size_t write(const void *data, size_t len);

  /**
   * @brief Writes to the ring buffer without overwriting oldest data.
   *
   * The provided data is written to the ring buffer. If not enough space is available,
   * the function will wait up to `ticks_to_wait` FreeRTOS ticks before writing as much as possible.
   *
   * @param data Pointer to data for writing
   * @param len Number of bytes to write
   * @param ticks_to_wait Maximum number of FreeRTOS ticks to wait (default: 0)
   * @return Number of bytes written
   */
  size_t write_without_replacement(const void *data, size_t len, TickType_t ticks_to_wait = 0,
                                   bool write_partial = true);

  /**
   * @brief Returns the number of available bytes in the ring buffer.
   *
   * This function provides the number of bytes that can be read from the ring buffer
   * without blocking the calling FreeRTOS task.
   *
   * @return Number of available bytes
   */
  size_t available() const;

  /**
   * @brief Returns the number of free bytes in the ring buffer.
   *
   * This function provides the number of bytes that can be written to the ring buffer
   * without overwriting data or blocking the calling FreeRTOS task.
   *
   * @return Number of free bytes
   */
  size_t free() const;

  /**
   * @brief Resets the ring buffer, discarding all stored data.
   *
   * @return pdPASS if successful, pdFAIL otherwise
   */
  BaseType_t reset();

  enum class MemoryPreference {
    EXTERNAL_FIRST,  // External RAM preferred, fall back to internal (default)
    INTERNAL_FIRST,  // Internal RAM preferred, fall back to external
  };

  static std::unique_ptr<RingBuffer> create(size_t len, MemoryPreference preference = MemoryPreference::EXTERNAL_FIRST);

 protected:
  /// @brief Discards data from the ring buffer.
  /// @param discard_bytes amount of bytes to discard
  /// @return True if all bytes were successfully discarded, false otherwise
  bool discard_bytes_(size_t discard_bytes);

  RingbufHandle_t handle_{nullptr};
  StaticRingbuffer_t structure_;
  uint8_t *storage_{nullptr};
  size_t size_{0};
};

}  // namespace esphome::ring_buffer

#endif  // USE_ESP32
