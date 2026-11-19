#include "stream_tap.h"

#include <algorithm>

namespace esphome::audio_analyzer {

void StreamTap::restart() {
  // A pending or acknowledged restart is already one, and right after a drain nothing old is left
  uint8_t sync = this->sync_.load(std::memory_order_acquire);
  while (sync == SYNC_STATE_COPYING &&
         !this->sync_.compare_exchange_weak(sync, SYNC_STATE_RESYNC, std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
  }
}

bool StreamTap::begin_write_() {
  uint8_t sync = this->sync_.load(std::memory_order_acquire);
  if (sync == SYNC_STATE_DRAINED) {
    this->decimator_.reset();
    // A restart requested meanwhile wins
    if (this->sync_.compare_exchange_strong(sync, SYNC_STATE_COPYING, std::memory_order_acq_rel)) {
      return true;
    }
  }
  if (sync == SYNC_STATE_COPYING) {
    return true;
  }
  this->acknowledge_restart_();
  return false;
}

bool StreamTap::flush_(const void *data, size_t bytes) {
  // Checked before every write, so a restart requested during a long write stops it at the next one
  if (this->sync_.load(std::memory_order_acquire) != SYNC_STATE_COPYING) {
    this->acknowledge_restart_();
    return false;
  }
  if (!this->buffer_->write(data, bytes)) {
    // Full: start over from the newest audio
    this->overflow_.store(true, std::memory_order_relaxed);
    this->sync_.store(SYNC_STATE_PAUSED, std::memory_order_release);
    return false;
  }
  return true;
}

void StreamTap::write(const uint8_t *frames, uint32_t count, uint8_t bytes_per_sample, uint8_t channels) {
  if (!this->begin_write_()) {
    return;
  }

  if (this->decimator_.get_factor() == 1 && bytes_per_sample == sizeof(int16_t) && channels == 1) {
    // Already 16-bit mono at the analysis rate (a 16 kHz microphone): copied as is
    while (count > 0) {
      const uint32_t samples = std::min(count, CHUNK_SAMPLES);
      if (!this->flush_(frames, samples * sizeof(int16_t))) {
        return;
      }
      frames += samples * sizeof(int16_t);
      count -= samples;
    }
    return;
  }

  const size_t frame_bytes = static_cast<size_t>(bytes_per_sample) * channels;
  uint32_t fill = 0;
  for (uint32_t f = 0; f < count; ++f) {
    float sample;
    if (!this->decimator_.push(read_mono_sample(frames + f * frame_bytes, bytes_per_sample, channels), sample)) {
      continue;
    }
    this->chunk_[fill++] = static_cast<int16_t>(std::clamp(sample * 32768.0f, -32768.0f, 32767.0f));
    if (fill == CHUNK_SAMPLES) {
      if (!this->flush_(this->chunk_.data(), fill * sizeof(int16_t))) {
        return;
      }
      fill = 0;
    }
  }
  if (fill > 0) {
    this->flush_(this->chunk_.data(), fill * sizeof(int16_t));
  }
}

bool StreamTap::drain_if_restarting() {
  // Only after the producer acknowledged: it writes nothing until it sees DRAINED
  if (this->sync_.load(std::memory_order_acquire) != SYNC_STATE_PAUSED) {
    return false;
  }
  this->buffer_->reset();
  this->sync_.store(SYNC_STATE_DRAINED, std::memory_order_release);
  return true;
}

}  // namespace esphome::audio_analyzer
