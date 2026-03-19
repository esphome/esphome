#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_PLAYER)

#include "esphome/components/sendspin/sendspin_protocol.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

namespace esphome {
namespace sendspin {

/// @brief Header for entries stored in the ring buffer.
/// Variable-length audio data follows immediately after this struct.
struct AudioRingBufferEntry {
  int64_t timestamp;
  ChunkType chunk_type;
  size_t data_size;

  uint8_t *data() { return reinterpret_cast<uint8_t *>(this) + sizeof(AudioRingBufferEntry); }
  const uint8_t *data() const { return reinterpret_cast<const uint8_t *>(this) + sizeof(AudioRingBufferEntry); }
};

/// @brief Pre-allocated ring buffer for audio chunks using FreeRTOS RINGBUF_TYPE_NOSPLIT.
///
/// Threading model: Single-Producer Single-Consumer (SPSC)
/// - Producer (WS callback thread) calls write_chunk()
/// - Consumer (sync task) calls receive_chunk() / return_chunk()
///
/// Zero per-chunk heap allocation: all data is stored contiguously in the pre-allocated ring buffer.
class SendspinAudioRingBuffer {
 public:
  /// @brief Creates a ring buffer with the specified total storage size.
  /// @param buffer_size Total ring buffer storage in bytes.
  /// @return unique_ptr to the ring buffer, or nullptr on allocation failure.
  static std::unique_ptr<SendspinAudioRingBuffer> create(size_t buffer_size);

  ~SendspinAudioRingBuffer();

  /// @brief Writes an audio chunk into the ring buffer.
  /// @param data Pointer to the audio data.
  /// @param data_size Size of the audio data in bytes.
  /// @param timestamp Server timestamp for this chunk.
  /// @param chunk_type Type of audio chunk.
  /// @param ticks_to_wait FreeRTOS ticks to wait if buffer is full.
  /// @return true if successfully written, false if buffer full or error.
  bool write_chunk(const uint8_t *data, size_t data_size, int64_t timestamp, ChunkType chunk_type,
                   TickType_t ticks_to_wait);

  /// @brief Receives the next audio chunk from the ring buffer.
  /// @param ticks_to_wait FreeRTOS ticks to wait if buffer is empty.
  /// @return Pointer to the entry, or nullptr if nothing available.
  /// @note Caller MUST call return_chunk() when done with the entry.
  AudioRingBufferEntry *receive_chunk(TickType_t ticks_to_wait);

  /// @brief Returns a previously received chunk to the ring buffer.
  /// @param entry Pointer previously returned by receive_chunk(). May be nullptr (no-op).
  void return_chunk(AudioRingBufferEntry *entry);

  /// @brief Drains all items from the ring buffer.
  /// @note Only safe to call when the consumer task is stopped.
  void reset();

 protected:
  SendspinAudioRingBuffer() = default;

  RingbufHandle_t ring_buffer_{nullptr};
  StaticRingbuffer_t structure_;
  uint8_t *storage_{nullptr};
  size_t size_{0};
};

}  // namespace sendspin
}  // namespace esphome

#endif
