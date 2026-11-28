#pragma once

#include <cstddef>
#include <memory>

namespace esphome::audio_analyzer {

/// Single-writer, single-reader byte buffer; a ``ring_buffer::RingBuffer`` on the device, a fake in the host tests.
class SampleBuffer {
 public:
  virtual ~SampleBuffer() = default;

  /// In PSRAM when available. Returns nullptr when out of memory.
  static std::unique_ptr<SampleBuffer> create(size_t size);

  // None of these wait. reset() is called by the reader while the writer doesn't write.
  /// Writes all ``length`` bytes, or nothing when they don't fit.
  virtual bool write(const void *data, size_t length) = 0;
  virtual size_t read(void *data, size_t length) = 0;
  virtual size_t available() const = 0;
  virtual void reset() = 0;
};

}  // namespace esphome::audio_analyzer
