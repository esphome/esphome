#include "audio.h"

#include "esphome/core/helpers.h"

#include <cstring>

namespace esphome::audio {

// Euclidean's algorithm for finding the greatest common divisor
static uint32_t gcd(uint32_t a, uint32_t b) {
  while (b != 0) {
    uint32_t t = b;
    b = a % b;
    a = t;
  }
  return a;
}

AudioStreamInfo::AudioStreamInfo(uint8_t bits_per_sample, uint8_t channels, uint32_t sample_rate)
    : bits_per_sample_(bits_per_sample), channels_(channels), sample_rate_(sample_rate) {
  this->ms_sample_rate_gcd_ = gcd(1000, this->sample_rate_);
  this->bytes_per_sample_ = (this->bits_per_sample_ + 7) / 8;
}

uint32_t AudioStreamInfo::frames_to_microseconds(uint32_t frames) const {
  return (frames * 1000000 + (this->sample_rate_ >> 1)) / this->sample_rate_;
}

uint32_t AudioStreamInfo::frames_to_milliseconds_with_remainder(uint32_t *total_frames) const {
  uint32_t unprocessable_frames = *total_frames % (this->sample_rate_ / this->ms_sample_rate_gcd_);
  uint32_t frames_for_ms_calculation = *total_frames - unprocessable_frames;

  uint32_t playback_ms = (frames_for_ms_calculation * 1000) / this->sample_rate_;
  *total_frames = unprocessable_frames;
  return playback_ms;
}

bool AudioStreamInfo::operator==(const AudioStreamInfo &rhs) const {
  return (this->bits_per_sample_ == rhs.get_bits_per_sample()) && (this->channels_ == rhs.get_channels()) &&
         (this->sample_rate_ == rhs.get_sample_rate());
}

const char *audio_file_type_to_string(AudioFileType file_type) {
  switch (file_type) {
#ifdef USE_AUDIO_FLAC_SUPPORT
    case AudioFileType::FLAC:
      return "FLAC";
#endif
#ifdef USE_AUDIO_MP3_SUPPORT
    case AudioFileType::MP3:
      return "MP3";
#endif
#ifdef USE_AUDIO_OPUS_SUPPORT
    case AudioFileType::OPUS:
      return "OPUS";
#endif
#ifdef USE_AUDIO_WAV_SUPPORT
    case AudioFileType::WAV:
      return "WAV";
#endif
    default:
      return "unknown";
  }
}

AudioFileType detect_audio_file_type(const char *content_type, const char *url) {
  // Try Content-Type header first
  if (content_type != nullptr && content_type[0] != '\0') {
#ifdef USE_AUDIO_MP3_SUPPORT
    if (strcasecmp(content_type, "mp3") == 0 || strcasecmp(content_type, "audio/mp3") == 0 ||
        strcasecmp(content_type, "audio/mpeg") == 0) {
      return AudioFileType::MP3;
    }
#endif
#ifdef USE_AUDIO_WAV_SUPPORT
    if (strcasecmp(content_type, "audio/wav") == 0) {
      return AudioFileType::WAV;
    }
#endif
#ifdef USE_AUDIO_FLAC_SUPPORT
    if (strcasecmp(content_type, "audio/flac") == 0 || strcasecmp(content_type, "audio/x-flac") == 0) {
      return AudioFileType::FLAC;
    }
#endif
#ifdef USE_AUDIO_OPUS_SUPPORT
    // Match "audio/ogg" with a codecs parameter containing "opus"
    // Valid forms: audio/ogg;codecs=opus, audio/ogg; codecs="opus", etc.
    // Plain "audio/ogg" without opus is not matched (almost always Ogg Vorbis)
    if (strncasecmp(content_type, "audio/ogg", 9) == 0 && strcasestr(content_type + 9, "opus") != nullptr) {
      return AudioFileType::OPUS;
    }
#endif
  }

  // Fallback to URL extension
  if (url != nullptr && url[0] != '\0') {
#ifdef USE_AUDIO_WAV_SUPPORT
    if (str_endswith_ignore_case(url, ".wav")) {
      return AudioFileType::WAV;
    }
#endif
#ifdef USE_AUDIO_MP3_SUPPORT
    if (str_endswith_ignore_case(url, ".mp3")) {
      return AudioFileType::MP3;
    }
#endif
#ifdef USE_AUDIO_FLAC_SUPPORT
    if (str_endswith_ignore_case(url, ".flac")) {
      return AudioFileType::FLAC;
    }
#endif
#ifdef USE_AUDIO_OPUS_SUPPORT
    if (str_endswith_ignore_case(url, ".opus")) {
      return AudioFileType::OPUS;
    }
#endif
  }

  return AudioFileType::NONE;
}

void scale_audio_samples(const int16_t *audio_samples, int16_t *output_buffer, int16_t scale_factor,
                         size_t samples_to_scale) {
  // Note the assembly dsps_mulc function has audio glitches if the input and output buffers are the same.
  for (size_t i = 0; i < samples_to_scale; i++) {
    int32_t acc = (int32_t) audio_samples[i] * (int32_t) scale_factor;
    output_buffer[i] = (int16_t) (acc >> 15);
  }
}

}  // namespace esphome::audio
