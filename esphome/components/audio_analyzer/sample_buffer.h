#pragma once

#include <cstddef>
#include <memory>

namespace esphome::audio_analyzer {

/// @brief The byte buffer between the task that receives the audio and loop(): one writer task, one reader task.
///
/// On the device it is a ``ring_buffer::RingBuffer``. The interface keeps ``StreamTap`` independent of FreeRTOS, so
/// the host unit tests run it against a plain buffer.
class SampleBuffer {
 public:
  virtual ~SampleBuffer() = default;

  /// @brief Allocates a buffer of ``size`` bytes, in PSRAM when available. Returns nullptr when out of memory.
  static std::unique_ptr<SampleBuffer> create(size_t size);

  /// @brief Writes all ``length`` bytes, or nothing when they don't fit. Never waits. THREAD CONTEXT: the writer.
  virtual bool write(const void *data, size_t length) = 0;
  /// @brief Reads up to ``length`` bytes, never waits. Returns the number read. THREAD CONTEXT: the reader.
  virtual size_t read(void *data, size_t length) = 0;
  /// @brief Number of bytes the reader can read. THREAD CONTEXT: the reader.
  virtual size_t available() const = 0;
  /// @brief Drops every buffered byte. THREAD CONTEXT: the reader, while the writer doesn't write.
  virtual void reset() = 0;
};

}  // namespace esphome::audio_analyzer
