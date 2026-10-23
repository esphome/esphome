#include "microphone_source.h"

#include <array>

#include <gain.h>         // esp-audio-libs
#include <pcm_convert.h>  // esp-audio-libs

namespace esphome::microphone {

// Builds the exact LinearGain for an integer factor without the float dB conversion: the factor is
// mantissa / 2^31 * 2^exponent, with the exponent set to the factor's bit width.
static esp_audio_libs::gain::LinearGain factor_to_linear_gain(uint32_t factor) {
  esp_audio_libs::gain::LinearGain gain;
  gain.exponent = 32 - __builtin_clz(factor);
  gain.mantissa_q31 = static_cast<int32_t>(factor << (31 - gain.exponent));
  return gain;
}

audio::AudioStreamInfo MicrophoneSource::get_audio_stream_info() {
  return audio::AudioStreamInfo(this->bits_per_sample_, this->channels_.count(),
                                this->mic_->get_audio_stream_info().get_sample_rate());
}

void MicrophoneSource::start() {
  if (!this->enabled_ && !this->passive_) {
    this->enabled_ = true;
    this->mic_->start();
  }
}

void MicrophoneSource::stop() {
  if (this->enabled_ && !this->passive_) {
    this->enabled_ = false;
    this->mic_->stop();
    this->processed_samples_.reset();
  }
}

void MicrophoneSource::process_audio_(const std::vector<uint8_t> &data, std::vector<uint8_t> &filtered_data) {
  const audio::AudioStreamInfo source_info = this->mic_->get_audio_stream_info();
  const uint32_t total_frames = source_info.bytes_to_frames(data.size());
  const uint8_t target_bytes_per_sample = (this->bits_per_sample_ + 7) / 8;

  // Enabled channels in ascending order, which is their order in each output frame
  std::array<uint8_t, 8> channel_map;
  uint8_t target_channels = 0;
  for (size_t channel = 0; channel < this->channels_.size(); ++channel) {
    if (this->channels_.test(channel)) {
      channel_map[target_channels++] = static_cast<uint8_t>(channel);
    }
  }

  filtered_data.resize(static_cast<size_t>(total_frames) * target_bytes_per_sample * target_channels);

  // Applies the gain at full precision before narrowing; samples saturate at full scale, and narrowing rounds
  esp_audio_libs::pcm_convert::copy_frames_with_gain(data.data(), filtered_data.data(), source_info.samples_to_bytes(1),
                                                     source_info.get_channels(), target_bytes_per_sample,
                                                     channel_map.data(), target_channels,
                                                     factor_to_linear_gain(this->gain_factor_), total_frames);
}

}  // namespace esphome::microphone
