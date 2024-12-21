#pragma once

#ifdef USE_ESP32

#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

#include <cinttypes>
#include <memory>

namespace esphome {

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
  size_t write_without_replacement(const void *data, size_t len, TickType_t ticks_to_wait = 0);

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

  static std::unique_ptr<RingBuffer> create(size_t len);

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

}  // namespace esphome

#endif
