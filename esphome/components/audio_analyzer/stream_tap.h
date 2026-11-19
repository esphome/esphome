#pragma once

// Platform-independent hand-over of audio from the task that receives it to loop(). No ESP32 or App dependency, so it
// runs in the host unit tests, with a plain buffer in place of the ring buffer.

#include "analysis.h"
#include "sample_buffer.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace esphome::audio_analyzer {

/// @brief Copies an audio stream into a ``SampleBuffer`` for the analysis in loop().
///
/// The producer (the task that receives the audio) downmixes each frame to mono, decimates it to the analysis rate
/// and writes it as 16-bit samples, without waiting. Each write to the buffer is all or nothing.
///
/// When the buffer is full, or ``restart()`` is called, the producer stops writing and acknowledges. Only then does
/// loop() empty the buffer (``drain_if_restarting()``), so no write still in progress can land after the drain. The
/// producer then starts again from the newest audio with a cleared filter. So between two drains, the samples loop()
/// reads are one continuous piece of the stream.
class StreamTap {
 public:
  /// @brief Allocates room for ``capacity`` samples. Returns false when out of memory.
  bool allocate(uint32_t capacity) {
    this->buffer_ = SampleBuffer::create(capacity * sizeof(int16_t));
    return this->buffer_ != nullptr;
  }
  bool is_allocated() const { return this->buffer_ != nullptr; }
  /// Set up by code generation, used by the producer only.
  Decimator &get_decimator() { return this->decimator_; }
  uint8_t get_decimation() const { return this->decimator_.get_factor(); }

  /// @brief Copies ``count`` whole interleaved little-endian frames. THREAD CONTEXT: producer.
  void write(const uint8_t *frames, uint32_t count, uint8_t bytes_per_sample, uint8_t channels);

  /// @brief Drops the buffered audio and restarts the copy from the producer's next write. Any context.
  void restart();

  // THREAD CONTEXT: consumer (loop())

  /// @brief Empties the buffer once the producer acknowledged a restart, letting it start again. Returns true if it
  /// did.
  bool drain_if_restarting();
  /// @brief Number of samples ready to read; 0 while a restart is pending.
  uint32_t available() const {
    if (this->sync_.load(std::memory_order_acquire) != SYNC_STATE_COPYING) {
      return 0;
    }
    return static_cast<uint32_t>(this->buffer_->available() / sizeof(int16_t));
  }
  /// @brief Calls ``on_sample(int16_t)`` for the next ``count`` samples (at most ``available()``) and frees them.
  template<typename F> void consume(uint32_t count, F &&on_sample) {
    std::array<int16_t, CHUNK_SAMPLES> chunk;
    while (count > 0) {
      const uint32_t want = count < CHUNK_SAMPLES ? count : CHUNK_SAMPLES;
      const uint32_t got =
          static_cast<uint32_t>(this->buffer_->read(chunk.data(), want * sizeof(int16_t)) / sizeof(int16_t));
      for (uint32_t i = 0; i < got; ++i) {
        on_sample(chunk[i]);
      }
      if (got < want) {
        return;
      }
      count -= got;
    }
  }
  /// @brief Frees the next ``count`` samples (at most ``available()``) unread.
  void skip(uint32_t count) {
    this->consume(count, [](int16_t) {});
  }
  /// True once since the producer last found the buffer full.
  bool take_overflow() { return this->overflow_.exchange(false, std::memory_order_relaxed); }

 protected:
  /// Samples per write to (and read from) the buffer
  static constexpr uint32_t CHUNK_SAMPLES = 128;

  enum SyncState : uint8_t {
    /// Restart requested; the producer acknowledges at its next check and stops writing
    SYNC_STATE_RESYNC,
    /// The producer doesn't write; loop() may empty the buffer
    SYNC_STATE_PAUSED,
    /// loop() emptied the buffer; the producer clears its filter and starts writing on its next call
    SYNC_STATE_DRAINED,
    /// The producer writes every frame
    SYNC_STATE_COPYING,
  };

  /// @brief Producer: true when this write copies, starting the copy after a drain.
  bool begin_write_();
  /// @brief Producer: writes ``bytes`` unless a restart is pending or they don't fit. Returns false when it stopped.
  bool flush_(const void *data, size_t bytes);
  /// @brief Producer: acknowledges a pending restart.
  void acknowledge_restart_() {
    uint8_t expected = SYNC_STATE_RESYNC;
    this->sync_.compare_exchange_strong(expected, SYNC_STATE_PAUSED, std::memory_order_acq_rel);
  }

  std::unique_ptr<SampleBuffer> buffer_;
  Decimator decimator_;
  std::array<int16_t, CHUNK_SAMPLES> chunk_{};  // producer only
  // Paused until loop() first drains
  std::atomic<uint8_t> sync_{SYNC_STATE_PAUSED};
  std::atomic<bool> overflow_{false};
};

}  // namespace esphome::audio_analyzer
