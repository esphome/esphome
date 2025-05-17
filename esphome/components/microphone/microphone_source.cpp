#include "microphone_source.h"

namespace esphome {
namespace microphone {

static const int32_t Q25_MAX_VALUE = (1 << 25) - 1;
static const int32_t Q25_MIN_VALUE = ~Q25_MAX_VALUE;

void MicrophoneSource::add_data_callback(std::function<void(const std::vector<uint8_t> &)> &&data_callback) {
  std::function<void(const std::vector<uint8_t> &)> filtered_callback =
      [this, data_callback](const std::vector<uint8_t> &data) {
        if (this->enabled_ || this->passive_) {
          if (this->processed_samples_.use_count() == 0) {
            // Create vector if its unused
            this->processed_samples_ = std::make_shared<std::vector<uint8_t>>();
          }

          // Take temporary ownership of samples vector to avoid deallaction before the callback finishes
          std::shared_ptr<std::vector<uint8_t>> output_samples = this->processed_samples_;
          this->process_audio_(data, *output_samples);
          data_callback(*output_samples);
        }
      };
  this->mic_->add_data_callback(std::move(filtered_callback));
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
  // - Bit depth conversions are obtained by truncating bits or padding with zeros - no dithering is applied.
  // - In the comments, Qxx refers to a fixed point number with xx bits of precision for representing fractional values.
  //   For example, audio with a bit depth of 16 can store a sample in a int16, which can be considered a Q15 number.
  // - All samples are converted to Q25 before applying the gain factor - this results in a small precision loss for
  //   data with 32 bits per sample. Since the maximum gain factor is 64 = (1<<6), this ensures that applying the gain
  //   will never overflow a 32 bit signed integer. This still retains more bit depth than what is audibly noticeable.
  // - Loops for reading/writing data buffers are unrolled, assuming little endian, for a small performance increase.

  const size_t source_bytes_per_sample = this->mic_->get_audio_stream_info().samples_to_bytes(1);
  const uint32_t source_channels = this->mic_->get_audio_stream_info().get_channels();

  const size_t source_bytes_per_frame = this->mic_->get_audio_stream_info().frames_to_bytes(1);

  const uint32_t total_frames = this->mic_->get_audio_stream_info().bytes_to_frames(data.size());
  const size_t target_bytes_per_sample = (this->bits_per_sample_ + 7) / 8;
  const size_t target_bytes_per_frame = target_bytes_per_sample * this->channels_.count();

  filtered_data.resize(target_bytes_per_frame * total_frames);

  uint8_t *current_data = filtered_data.data();

  for (uint32_t frame_index = 0; frame_index < total_frames; ++frame_index) {
    for (uint32_t channel_index = 0; channel_index < source_channels; ++channel_index) {
      if (this->channels_.test(channel_index)) {
        // Channel's current sample is included in the target mask. Convert bits per sample, if necessary.

        const uint32_t sample_index = frame_index * source_bytes_per_frame + channel_index * source_bytes_per_sample;

        int32_t sample = audio::unpack_audio_sample_to_q31(&data[sample_index], source_bytes_per_sample);  // Q31
        sample >>= 6;                                                                                      // Q31 -> Q25

        // Apply gain using multiplication
        sample *= this->gain_factor_;  // Q25

        // Clamp ``sample`` in case gain multiplication overflows 25 bits
        sample = clamp<int32_t>(sample, Q25_MIN_VALUE, Q25_MAX_VALUE);  // Q25

        sample *= (1 << 6);  // Q25 -> Q31

        audio::pack_q31_as_audio_sample(sample, current_data, target_bytes_per_sample);
        current_data = current_data + target_bytes_per_sample;
      }
    }
  }
}

}  // namespace microphone
}  // namespace esphome
