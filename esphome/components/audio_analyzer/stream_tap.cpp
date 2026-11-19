#include "stream_tap.h"

#include "esphome/core/helpers.h"

#include <algorithm>

namespace esphome::audio_analyzer {

bool SampleFifo::allocate(uint32_t capacity) {
  RAMAllocator<int16_t> allocator;
  this->data_ = allocator.allocate(capacity);
  this->capacity_ = this->data_ != nullptr ? capacity : 0;
  return this->data_ != nullptr;
}

void StreamTap::write(const uint8_t *frames, uint32_t count, uint8_t bytes_per_sample, uint8_t channels) {
  uint8_t sync = this->sync_.load(std::memory_order_acquire);
  if (sync == SYNC_STATE_DRAINED) {
    this->decimator_.reset();
    // A restart requested meanwhile wins
    if (!this->sync_.compare_exchange_strong(sync, SYNC_STATE_COPYING, std::memory_order_acq_rel)) {
      return;
    }
    sync = SYNC_STATE_COPYING;
  }
  if (sync != SYNC_STATE_COPYING) {
    return;
  }

  const size_t frame_bytes = static_cast<size_t>(bytes_per_sample) * channels;
  for (uint32_t f = 0; f < count; ++f) {
    float sample;
    if (!this->decimator_.push(read_mono_sample(frames + f * frame_bytes, bytes_per_sample, channels), sample)) {
      continue;
    }
    if (!this->fifo_.push(static_cast<int16_t>(std::clamp(sample * 32768.0f, -32768.0f, 32767.0f)))) {
      // Full: loop() fell behind, or the output holds more audio than the FIFO. Start over from the newest audio.
      this->overflow_.store(true, std::memory_order_relaxed);
      this->sync_.store(SYNC_STATE_RESYNC, std::memory_order_release);
      break;
    }
  }
  this->fifo_.publish();
}

bool StreamTap::drain_if_restarting() {
  uint8_t expected = SYNC_STATE_RESYNC;
  if (this->sync_.load(std::memory_order_acquire) != expected) {
    return false;
  }
  this->fifo_.clear();
  this->sync_.compare_exchange_strong(expected, SYNC_STATE_DRAINED, std::memory_order_acq_rel);
  return true;
}

}  // namespace esphome::audio_analyzer
