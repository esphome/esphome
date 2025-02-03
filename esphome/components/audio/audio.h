#pragma once

#include "esphome/core/defines.h"

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace audio {

class AudioStreamInfo {
  /* Class to respresent important parameters of the audio stream that also provides helper function to convert between
   * various audio related units.
   *
   *  - An audio sample represents a unit of audio for one channel.
   *  - A frame represents a unit of audio with a sample for every channel.
   *
   * In gneneral, converting between bytes, samples, and frames shouldn't result in rounding errors so long as frames
   * are used as the main unit when transferring audio data. Durations may result in rounding for certain sample rates;
   * e.g., 44.1 KHz. The ``frames_to_milliseconds_with_remainder`` function should be used for accuracy, as it takes
   * into account the remainder rather than just ignoring any rounding.
   */
 public:
  AudioStreamInfo()
      : AudioStreamInfo(16, 1, 16000){};  // Default values represent ESPHome's audio components historical values
  AudioStreamInfo(uint8_t bits_per_sample, uint8_t channels, uint32_t sample_rate);

  uint8_t get_bits_per_sample() const { return this->bits_per_sample_; }
  uint8_t get_channels() const { return this->channels_; }
  uint32_t get_sample_rate() const { return this->sample_rate_; }

  /// @brief Convert bytes to duration in milliseconds.
  /// @param bytes Number of bytes to convert
  /// @return Duration in milliseconds that will store `bytes` bytes of audio. May round down for certain sample rates
  ///         or values of `bytes`.
  uint32_t bytes_to_ms(size_t bytes) const {
    return bytes * 1000 / (this->sample_rate_ * this->bytes_per_sample_ * this->channels_);
  }

  /// @brief Convert bytes to frames.
  /// @param bytes Number of bytes to convert
  /// @return Audio frames that will store `bytes` bytes.
  uint32_t bytes_to_frames(size_t bytes) const { return (bytes / (this->bytes_per_sample_ * this->channels_)); }

  /// @brief Convert bytes to samples.
  /// @param bytes Number of bytes to convert
  /// @return Audio samples that will store `bytes` bytes.
  uint32_t bytes_to_samples(size_t bytes) const { return (bytes / this->bytes_per_sample_); }

  /// @brief Converts frames to bytes.
  /// @param frames Number of frames to convert.
  /// @return Number of bytes that will store `frames` frames of audio.
  size_t frames_to_bytes(uint32_t frames) const { return frames * this->bytes_per_sample_ * this->channels_; }

  /// @brief Converts samples to bytes.
  /// @param samples Number of samples to convert.
  /// @return Number of bytes that will store `samples` samples of audio.
  size_t samples_to_bytes(uint32_t samples) const { return samples * this->bytes_per_sample_; }

  /// @brief Converts duration to frames.
  /// @param ms Duration in milliseconds
  /// @return Audio frames that will store `ms` milliseconds of audio.  May round down for certain sample rates.
  uint32_t ms_to_frames(uint32_t ms) const { return (ms * this->sample_rate_) / 1000; }

  /// @brief Converts duration to samples.
  /// @param ms Duration in milliseconds
  /// @return Audio samples that will store `ms` milliseconds of audio.  May round down for certain sample rates.
  uint32_t ms_to_samples(uint32_t ms) const { return (ms * this->channels_ * this->sample_rate_) / 1000; }

  /// @brief Converts duration to bytes. May round down for certain sample rates.
  /// @param ms Duration in milliseconds
  /// @return Bytes that will store `ms` milliseconds of audio.  May round down for certain sample rates.
  size_t ms_to_bytes(uint32_t ms) const {
    return (ms * this->bytes_per_sample_ * this->channels_ * this->sample_rate_) / 1000;
  }

  /// @brief Computes the duration, in microseconds, the given amount of frames represents.
  /// @param frames Number of audio frames
  /// @return Duration in microseconds `frames` respresents. May be slightly inaccurate due to integer divison rounding
  ///         for certain sample rates.
  uint32_t frames_to_microseconds(uint32_t frames) const;

  /// @brief Computes the duration, in milliseconds, the given amount of frames represents. Avoids
  /// accumulating rounding errors by updating `frames` with the remainder after converting.
  /// @param frames Pointer to uint32_t with the number of audio frames. Replaced with the remainder.
  /// @return Duration in milliseconds `frames` represents. Always less than or equal to the actual value due to
  ///         rounding.
  uint32_t frames_to_milliseconds_with_remainder(uint32_t *frames) const;

  // Class comparison operators
  bool operator==(const AudioStreamInfo &rhs) const;
  bool operator!=(const AudioStreamInfo &rhs) const { return !operator==(rhs); }

 protected:
  uint8_t bits_per_sample_;
  uint8_t channels_;
  uint32_t sample_rate_;

  // The greatest common divisor between 1000 ms = 1 second and the sample rate. Used to avoid accumulating error when
  // converting from frames to duration. Computed at construction.
  uint32_t ms_sample_rate_gcd_;

  // Conversion factor derived from the number of bits per sample. Assumes audio data is aligned to the byte. Computed
  // at construction.
  size_t bytes_per_sample_;
};

enum class AudioFileType : uint8_t {
  NONE = 0,
#ifdef USE_AUDIO_FLAC_SUPPORT
  FLAC,
#endif
#ifdef USE_AUDIO_MP3_SUPPORT
  MP3,
#endif
  WAV,
};

struct AudioFile {
  const uint8_t *data;
  size_t length;
  AudioFileType file_type;
};

/// @brief Helper function to convert file type to a const char string
/// @param file_type
/// @return const char pointer to the readable file type
const char *audio_file_type_to_string(AudioFileType file_type);

/// @brief Scales Q15 fixed point audio samples. Scales in place if audio_samples == output_buffer.
/// @param audio_samples PCM int16 audio samples
/// @param output_buffer Buffer to store the scaled samples
/// @param scale_factor Q15 fixed point scaling factor
/// @param samples_to_scale Number of samples to scale
void scale_audio_samples(const int16_t *audio_samples, int16_t *output_buffer, int16_t scale_factor,
                         size_t samples_to_scale);

}  // namespace audio
}  // namespace esphome
