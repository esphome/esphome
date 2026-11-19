#pragma once

#include "analysis.h"
#include "sample_buffer.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace esphome::audio_analyzer {

/// Hands audio from the task that receives it (producer) to loop() (consumer) as 16-bit mono samples at the analysis
/// rate, without waiting.
///
/// On overflow or ``restart()`` the producer stops and acknowledges; only then does loop() empty the buffer, and the
/// producer restarts from the newest audio. So between two drains, loop() reads one continuous piece of the stream.
class StreamTap {
 public:
  /// Returns false when out of memory.
  bool allocate(uint32_t capacity) {
    this->buffer_ = SampleBuffer::create(capacity * sizeof(int16_t));
    return this->buffer_ != nullptr;
  }
  bool is_allocated() const { return this->buffer_ != nullptr; }
  Decimator &get_decimator() { return this->decimator_; }
  uint8_t get_decimation() const { return this->decimator_.get_factor(); }

  /// Copies ``count`` interleaved little-endian frames. THREAD CONTEXT: producer.
  void write(const uint8_t *frames, uint32_t count, uint8_t bytes_per_sample, uint8_t channels);

  /// Drops the buffered audio. Any context.
  void restart();

  // THREAD CONTEXT: consumer (loop())

  /// Empties the buffer once the producer acknowledged a restart. Returns true if it did.
  bool drain_if_restarting();
  /// 0 while a restart is pending.
  uint32_t available() const {
    if (this->sync_.load(std::memory_order_acquire) != SYNC_STATE_COPYING) {
      return 0;
    }
    return static_cast<uint32_t>(this->buffer_->available() / sizeof(int16_t));
  }
  /// Calls ``on_sample(int16_t)`` for the next ``count`` samples.
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
  void skip(uint32_t count) {
    this->consume(count, [](int16_t) {});
  }
  /// True once after each overflow.
  bool take_overflow() { return this->overflow_.exchange(false, std::memory_order_relaxed); }

 protected:
  static constexpr uint32_t CHUNK_SAMPLES = 128;

  enum SyncState : uint8_t {
    SYNC_STATE_RESYNC,   // restart requested, not yet acknowledged
    SYNC_STATE_PAUSED,   // the producer doesn't write; loop() may drain
    SYNC_STATE_DRAINED,  // drained; the producer resets its filter and copies again
    SYNC_STATE_COPYING,
  };

  // Producer side
  bool begin_write_();
  /// Returns false when it stopped copying.
  bool flush_(const void *data, size_t bytes);
  void acknowledge_restart_() {
    uint8_t expected = SYNC_STATE_RESYNC;
    this->sync_.compare_exchange_strong(expected, SYNC_STATE_PAUSED, std::memory_order_acq_rel);
  }

  std::unique_ptr<SampleBuffer> buffer_;
  Decimator decimator_;
  std::array<int16_t, CHUNK_SAMPLES> chunk_{};
  // Paused until loop() first drains
  std::atomic<uint8_t> sync_{SYNC_STATE_PAUSED};
  std::atomic<bool> overflow_{false};
};

}  // namespace esphome::audio_analyzer
