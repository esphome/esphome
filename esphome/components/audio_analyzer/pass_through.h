#pragma once

// Platform-independent bookkeeping of the pass-through speaker, run by the host unit tests.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::audio_analyzer {

/// Buffered samples (``decimation`` frames each) older than the ``frames_in_output`` not played yet: what has been
/// heard. Frames written while nothing was copied hold the analysis back until played. The producer counts frames
/// before copying them, so read ``buffered_samples`` before ``frames_in_output``.
inline uint32_t samples_heard(uint32_t buffered_samples, uint8_t decimation, uint32_t frames_in_output) {
  const uint64_t buffered_frames = static_cast<uint64_t>(buffered_samples) * decimation;
  if (buffered_frames <= frames_in_output) {
    return 0;
  }
  return static_cast<uint32_t>((buffered_frames - frames_in_output) / decimation);
}

/// Rebuilds whole frames from speaker writes that can end mid-frame, keeping the partial frame for the next write.
class FrameAssembler {
 public:
  /// Largest frame kept: two 32-bit channels.
  static constexpr size_t MAX_FRAME_BYTES = 8;

  void reset() { this->partial_bytes_ = 0; }

  /// Calls ``on_frames(const uint8_t *frames, uint32_t count)`` for the whole frames, in stream order (a completed
  /// kept frame first). Returns the number of whole frames.
  template<typename F> uint32_t add(const uint8_t *data, size_t length, size_t frame_bytes, F &&on_frames) {
    if (frame_bytes == 0 || frame_bytes > MAX_FRAME_BYTES) {
      return 0;
    }
    if (this->partial_bytes_ >= frame_bytes) {
      this->partial_bytes_ = 0;  // left from a larger format
    }
    uint32_t frames = 0;
    if (this->partial_bytes_ > 0) {
      const size_t take = frame_bytes - this->partial_bytes_ < length ? frame_bytes - this->partial_bytes_ : length;
      std::memcpy(this->partial_ + this->partial_bytes_, data, take);
      this->partial_bytes_ += take;
      data += take;
      length -= take;
      if (this->partial_bytes_ < frame_bytes) {
        return 0;
      }
      this->partial_bytes_ = 0;
      on_frames(static_cast<const uint8_t *>(this->partial_), static_cast<uint32_t>(1));
      frames = 1;
    }
    const uint32_t whole = static_cast<uint32_t>(length / frame_bytes);
    if (whole > 0) {
      on_frames(data, whole);
    }
    this->partial_bytes_ = length - whole * frame_bytes;
    std::memcpy(this->partial_, data + whole * frame_bytes, this->partial_bytes_);
    return frames + whole;
  }

 protected:
  uint8_t partial_[MAX_FRAME_BYTES]{};
  size_t partial_bytes_{0};
};

}  // namespace esphome::audio_analyzer
