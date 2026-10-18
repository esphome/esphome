#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <limits>

namespace esphome::mixer_speaker {

// Metadata for the existing PCM buffers. The mixer serializes append, consume
// and discard; pending counts can also be read by the source speakers' loop().
class PlaybackQueue {
 public:
  // Keep in sync with the source_speakers schema limit in speaker/__init__.py.
  static constexpr uint8_t MAX_SOURCES = 8;
  static constexpr uint8_t CAPACITY = 32;

  struct Completion {
    uint32_t frames{0};
    uint32_t trailing_frames{0};
  };
  using Completions = std::array<Completion, MAX_SOURCES>;

  bool append(uint8_t sources, uint32_t frames) {
    if (frames == 0)
      return true;
    if (frames > std::numeric_limits<uint32_t>::max() - this->pending_frames())
      return false;
    for (uint8_t i = 0, mask = sources; mask != 0; ++i, mask >>= 1) {
      if ((mask & 1U) && frames > std::numeric_limits<uint32_t>::max() - this->pending_frames(i))
        return false;
    }
    const uint8_t tail = (this->head_ + this->size_ + CAPACITY - 1) % CAPACITY;
    if (this->size_ != 0 && this->sources_[tail] == sources) {
      this->frames_[tail] += frames;
    } else {
      if (this->size_ == CAPACITY)
        return false;
      const uint8_t next = (this->head_ + this->size_) % CAPACITY;
      this->frames_[next] = frames;
      this->sources_[next] = sources;
      ++this->size_;
    }
    this->pending_.fetch_add(frames, std::memory_order_release);
    for (uint8_t i = 0, mask = sources; mask != 0; ++i, mask >>= 1) {
      if (mask & 1U)
        this->source_pending_[i].fetch_add(frames, std::memory_order_release);
    }
    return true;
  }

  Completions consume(uint32_t frames) {
    Completions result{};
    uint32_t remaining = frames;
    while (remaining != 0 && this->size_ != 0) {
      auto &run_frames = this->frames_[this->head_];
      const uint32_t played = std::min(run_frames, remaining);
      run_frames -= played;
      remaining -= played;
      for (uint8_t i = 0, mask = this->sources_[this->head_]; mask != 0; ++i, mask >>= 1) {
        if (mask & 1U) {
          result[i].frames += played;
          result[i].trailing_frames = remaining;
        }
      }
      if (run_frames == 0) {
        this->head_ = (this->head_ + 1) % CAPACITY;
        --this->size_;
      }
    }
    this->pending_.fetch_sub(frames - remaining, std::memory_order_release);
    return result;
  }

  void acknowledge(uint8_t source, uint32_t frames) {
    // Keep the source pending until its listeners have processed the completion.
    this->source_pending_[source].fetch_sub(frames, std::memory_order_release);
  }

  void discard_source(uint8_t source) {
    // Its old PCM is still in the shared output. Keep its position, but do not
    // credit those frames to a later playback of this source.
    uint32_t discarded = 0;
    for (uint8_t i = 0; i < this->size_; ++i) {
      const uint8_t index = (this->head_ + i) % CAPACITY;
      if (this->sources_[index] & (1U << source)) {
        discarded += this->frames_[index];
        this->sources_[index] &= ~(1U << source);
      }
    }
    if (discarded != 0)
      this->source_pending_[source].fetch_sub(discarded, std::memory_order_release);
  }

  void clear() {
    for (uint8_t i = 0; i < MAX_SOURCES; ++i)
      this->discard_source(i);
    this->head_ = 0;
    this->size_ = 0;
    this->pending_.store(0, std::memory_order_release);
  }

  uint32_t pending_frames() const { return this->pending_.load(std::memory_order_acquire); }
  bool full() const { return this->size_ == CAPACITY; }
  uint32_t pending_frames(uint8_t source) const {
    return this->source_pending_[source].load(std::memory_order_acquire);
  }

 protected:
  // Separate arrays avoid three padding bytes per run while keeping frame counts aligned.
  std::array<uint32_t, CAPACITY> frames_{};
  std::array<uint8_t, CAPACITY> sources_{};
  // Includes frames removed from the queue whose callbacks have not returned yet.
  std::array<std::atomic<uint32_t>, MAX_SOURCES> source_pending_{};
  std::atomic<uint32_t> pending_{0};
  uint8_t head_{0};
  uint8_t size_{0};
};

}  // namespace esphome::mixer_speaker
