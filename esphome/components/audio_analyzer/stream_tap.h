#pragma once

// Platform-independent hand-over of audio from the task that receives it to loop(). No ESP32 or App dependency, so it
// runs in the host unit tests.

#include "analysis.h"

#include <atomic>
#include <cstdint>

namespace esphome::audio_analyzer {

/// @brief Lock-free single-producer, single-consumer FIFO of 16-bit samples, allocated once.
///
/// Each side owns one position and only reads the other's, so neither side ever waits or takes a lock. Positions run
/// freely and wrap around 2^32; the capacity is far below that.
class SampleFifo {
 public:
  /// @brief Allocates room for ``capacity`` samples, in PSRAM when available. Returns false when out of memory.
  bool allocate(uint32_t capacity);
  bool is_allocated() const { return this->data_ != nullptr; }
  uint32_t get_capacity() const { return this->capacity_; }

  // THREAD CONTEXT: producer

  /// @brief Appends a sample, visible to the consumer after ``publish()``. Returns false, writing nothing, when full.
  bool push(int16_t sample) {
    if (this->write_pos_ - this->read_pos_.load(std::memory_order_acquire) >= this->capacity_) {
      return false;
    }
    this->data_[this->write_pos_ % this->capacity_] = sample;
    ++this->write_pos_;
    return true;
  }
  void publish() { this->written_.store(this->write_pos_, std::memory_order_release); }

  // THREAD CONTEXT: consumer

  uint32_t available() const {
    return this->written_.load(std::memory_order_acquire) - this->read_pos_.load(std::memory_order_relaxed);
  }
  /// @brief Calls ``on_sample(int16_t)`` for the next ``count`` samples (at most ``available()``) and frees them.
  template<typename F> void consume(uint32_t count, F &&on_sample) {
    const uint32_t start = this->read_pos_.load(std::memory_order_relaxed);
    for (uint32_t i = 0; i < count; ++i) {
      on_sample(this->data_[(start + i) % this->capacity_]);
    }
    this->read_pos_.store(start + count, std::memory_order_release);
  }
  /// @brief Frees the next ``count`` samples (at most ``available()``) unread.
  void skip(uint32_t count) {
    this->read_pos_.store(this->read_pos_.load(std::memory_order_relaxed) + count, std::memory_order_release);
  }
  /// @brief Frees every published sample.
  void clear() { this->read_pos_.store(this->written_.load(std::memory_order_acquire), std::memory_order_release); }

 protected:
  int16_t *data_{nullptr};
  uint32_t capacity_{0};
  uint32_t write_pos_{0};  // producer only; ahead of written_ until published
  std::atomic<uint32_t> written_{0};
  std::atomic<uint32_t> read_pos_{0};
};

/// @brief Copies an audio stream into a ``SampleFifo`` for the analysis in loop().
///
/// The producer (the task that receives the audio) downmixes each frame to mono, decimates it to the analysis rate
/// and writes it as a 16-bit sample, without blocking. When the FIFO is full, or ``restart()`` is called, the producer
/// stops writing until loop() has emptied the FIFO (``drain_if_restarting()``), then starts again from the newest
/// audio with a cleared filter. So the samples loop() reads are always one continuous piece of the stream.
class StreamTap {
 public:
  bool allocate(uint32_t capacity) { return this->fifo_.allocate(capacity); }
  bool is_allocated() const { return this->fifo_.is_allocated(); }
  uint32_t get_capacity() const { return this->fifo_.get_capacity(); }
  /// Set up by code generation, used by the producer only.
  Decimator &get_decimator() { return this->decimator_; }
  uint8_t get_decimation() const { return this->decimator_.get_factor(); }

  /// @brief Copies ``count`` whole interleaved frames. THREAD CONTEXT: producer.
  void write(const uint8_t *frames, uint32_t count, uint8_t bytes_per_sample, uint8_t channels);

  /// @brief Drops the buffered audio and restarts the copy from the producer's next write. Any context.
  void restart() { this->sync_.store(SYNC_STATE_RESYNC, std::memory_order_release); }

  // THREAD CONTEXT: consumer (loop())

  /// @brief Empties the FIFO when a restart is pending, letting the producer start again. Returns true if it did.
  bool drain_if_restarting();
  /// True once since the producer last found the FIFO full.
  bool take_overflow() { return this->overflow_.exchange(false, std::memory_order_relaxed); }
  SampleFifo &get_fifo() { return this->fifo_; }

 protected:
  enum SyncState : uint8_t {
    /// Restart requested; the producer doesn't write
    SYNC_STATE_RESYNC,
    /// loop() emptied the FIFO; the producer clears its filter and starts writing on its next call
    SYNC_STATE_DRAINED,
    /// The producer writes every frame
    SYNC_STATE_COPYING,
  };

  SampleFifo fifo_;
  Decimator decimator_;
  std::atomic<uint8_t> sync_{SYNC_STATE_RESYNC};
  std::atomic<bool> overflow_{false};
};

}  // namespace esphome::audio_analyzer
