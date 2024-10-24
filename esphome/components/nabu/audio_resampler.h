#pragma once

#ifdef USE_ESP_IDF

#include "biquad.h"
#include "resampler.h"

#include "esphome/components/audio/audio.h"
#include "esphome/core/ring_buffer.h"

namespace esphome {
namespace nabu {

enum class AudioResamplerState : uint8_t {
  INITIALIZED = 0,
  RESAMPLING,
  FINISHED,
  FAILED,
};

struct ResampleInfo {
  bool resample;
  bool mono_to_stereo;
};

class AudioResampler {
 public:
  AudioResampler(esphome::RingBuffer *input_ring_buffer, esphome::RingBuffer *output_ring_buffer,
                 size_t internal_buffer_samples);
  ~AudioResampler();

  /// @brief Sets up the various bits necessary to resample
  /// @param stream_info the incoming sample rate, bits per sample, and number of channels
  /// @param target_sample_rate the necessary sample rate to convert to
  /// @return ESP_OK if it is able to convert the incoming stream or an error otherwise
  esp_err_t start(audio::AudioStreamInfo &stream_info, uint32_t target_sample_rate, ResampleInfo &resample_info);

  AudioResamplerState resample(bool stop_gracefully);

 protected:
  esp_err_t allocate_buffers_();

  esphome::RingBuffer *input_ring_buffer_;
  esphome::RingBuffer *output_ring_buffer_;
  size_t internal_buffer_samples_;

  int16_t *input_buffer_{nullptr};
  int16_t *input_buffer_current_{nullptr};
  size_t input_buffer_length_;

  int16_t *output_buffer_{nullptr};
  int16_t *output_buffer_current_{nullptr};
  size_t output_buffer_length_;

  float *float_input_buffer_{nullptr};
  float *float_input_buffer_current_{nullptr};
  size_t float_input_buffer_length_;

  float *float_output_buffer_{nullptr};
  float *float_output_buffer_current_{nullptr};
  size_t float_output_buffer_length_;

  audio::AudioStreamInfo stream_info_;
  ResampleInfo resample_info_;

  Resample *resampler_{nullptr};

  Biquad lowpass_[2][2];
  BiquadCoefficients lowpass_coeff_;

  float sample_ratio_{1.0};
  float lowpass_ratio_{1.0};
  uint8_t channel_factor_{1};

  bool pre_filter_{false};
  bool post_filter_{false};
};

}  // namespace nabu
}  // namespace esphome

#endif
