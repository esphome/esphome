#pragma once

// Platform-independent bookkeeping of the pass-through speaker: whole frames from partial writes and the samples that
// have been heard. No ESP32 or App dependency, so it runs in the host unit tests.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::audio_analyzer {

/// @brief Decimated samples due for analysis in a pass-through speaker.
///
/// The analysis buffer holds the newest copied audio, up to the write head: ``buffered_samples`` decimated samples,
/// ``decimation`` stream frames each. ``frames_in_output`` frames were written to the output speaker but not played
/// yet. Everything in the buffer older than those frames has been heard. Frames written while nothing was copied
/// are part of ``frames_in_output`` but not of the buffer, so they hold the analysis back until they are played.
///
/// The producer counts frames before it copies their samples, so the caller reads ``buffered_samples`` first and
/// ``frames_in_output`` second; then every buffered sample is counted.
inline uint32_t samples_heard(uint32_t buffered_samples, uint8_t decimation, uint32_t frames_in_output) {
  const uint64_t buffered_frames = static_cast<uint64_t>(buffered_samples) * decimation;
  if (buffered_frames <= frames_in_output) {
    return 0;
  }
  return static_cast<uint32_t>((buffered_frames - frames_in_output) / decimation);
}

/// @brief Rebuilds whole frames from writes that can end in the middle of a frame.
///
/// Speakers accept partial writes, so a write can end on any byte and the producer continues from there. The bytes
/// of an unfinished frame are kept until the next write completes it.
class FrameAssembler {
 public:
  /// Largest frame kept: two 32-bit channels.
  static constexpr size_t MAX_FRAME_BYTES = 8;

  /// @brief Drops a kept partial frame (new stream or format).
  void reset() { this->partial_bytes_ = 0; }

  /// @brief Splits ``length`` bytes into whole frames of ``frame_bytes``.
  ///
  /// Calls ``on_frames(const uint8_t *frames, uint32_t count)`` at most twice, in stream order: once for a kept
  /// frame this write completes, once for the whole frames that follow. Returns the number of whole frames.
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
