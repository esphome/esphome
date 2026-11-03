#pragma once

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#include <memory>

// The response tests substitute PCM capture and the FreeRTOS-backed buffers.
// VoiceAssistant, its scheduler and the API are compiled from production sources.
namespace esphome::ring_buffer {
class RingBuffer {
 public:
  static std::unique_ptr<RingBuffer> create(size_t) { return std::make_unique<RingBuffer>(); }
  size_t write(const void *, size_t) { return 0; }
};
}  // namespace esphome::ring_buffer

namespace esphome::audio {
class RingBufferAudioSource {
 public:
  static std::unique_ptr<RingBufferAudioSource> create(const std::shared_ptr<ring_buffer::RingBuffer> &, size_t,
                                                       size_t) {
    return std::make_unique<RingBufferAudioSource>();
  }
  size_t fill(uint32_t) { return 0; }
  size_t available() const { return 0; }
  const uint8_t *data() const { return nullptr; }
  void consume(size_t) {}
  void clear_buffered_data() {}
};
}  // namespace esphome::audio
#endif
