#include "microphone_source.h"

namespace esphome {
namespace microphone {

void MicrophoneSource::add_data_callback(std::function<void(const std::vector<uint8_t> &)> &&data_callback) {
  std::function<void(const std::vector<uint8_t> &)> filtered_callback =
      [this, data_callback](const std::vector<uint8_t> &data) {
        if (this->enabled_) {
          data_callback(this->process_audio_(data));
        }
      };
  // Future PR will uncomment this! It requires changing the callback vector to an uint8_t in every component using a
  // mic callback.
  // this->mic_->add_data_callback(std::move(filtered_callback));
}

void MicrophoneSource::start() {
  this->enabled_ = true;
  this->mic_->start();
}
void MicrophoneSource::stop() {
  this->enabled_ = false;
  this->mic_->stop();
}

std::vector<uint8_t> MicrophoneSource::process_audio_(const std::vector<uint8_t> &data) {
  // Bit depth conversions are obtained by truncating bits or padding with zeros - no dithering is applied.

  const size_t source_bytes_per_sample = this->mic_->get_audio_stream_info().samples_to_bytes(1);
  const size_t source_channels = this->mic_->get_audio_stream_info().get_channels();

  const size_t source_bytes_per_frame = this->mic_->get_audio_stream_info().frames_to_bytes(1);

  const uint32_t total_frames = this->mic_->get_audio_stream_info().bytes_to_frames(data.size());
  const size_t target_bytes_per_sample = (this->bits_per_sample_ + 7) / 8;
  const size_t target_bytes_per_frame = target_bytes_per_sample * this->channels_.count();

  std::vector<uint8_t> filtered_data;
  filtered_data.reserve(target_bytes_per_frame * total_frames);

  const int32_t target_min_value = -(1 << (8 * target_bytes_per_sample - 1));
  const int32_t target_max_value = (1 << (8 * target_bytes_per_sample - 1)) - 1;

  for (size_t frame_index = 0; frame_index < total_frames; ++frame_index) {
    for (size_t channel_index = 0; channel_index < source_channels; ++channel_index) {
      if (this->channels_.test(channel_index)) {
        // Channel's current sample is included in the target mask. Convert bits per sample, if necessary.

        size_t sample_index = frame_index * source_bytes_per_frame + channel_index * source_bytes_per_sample;

        int32_t sample = 0;

        // Copy the data into the most significant bits of the sample variable to ensure the sign bit is correct
        uint8_t bit_offset = (4 - source_bytes_per_sample) * 8;
        for (int i = 0; i < source_bytes_per_sample; ++i) {
          sample |= data[sample_index + i] << bit_offset;
          bit_offset += 8;
        }

        // Shift data back to the least significant bits
        if (source_bytes_per_sample >= target_bytes_per_sample) {
          // Keep source bytes per sample of data so that the gain multiplication uses all significant bits instead of
          // shifting to the target bytes per sample immediately, potentially losing information.
          sample >>= (4 - source_bytes_per_sample) * 8;  // ``source_bytes_per_sample`` bytes of valid data
        } else {
          // Keep padded zeros to match the target bytes per sample
          sample >>= (4 - target_bytes_per_sample) * 8;  // ``target_bytes_per_sample`` bytes of valid data
        }

        // Apply gain using multiplication
        sample *= this->gain_factor_;

        // Match target output bytes by shifting out the least significant bits
        if (source_bytes_per_sample > target_bytes_per_sample) {
          sample >>= 8 * (source_bytes_per_sample -
                          target_bytes_per_sample);  //  ``target_bytes_per_sample`` bytes of valid data
        }

        // Clamp ``sample`` to the target bytes per sample range in case gain multiplication overflows
        sample = clamp<int32_t>(sample, target_min_value, target_max_value);

        // Copy ``target_bytes_per_sample`` bytes to the output buffer.
        for (int i = 0; i < target_bytes_per_sample; ++i) {
          filtered_data.push_back(static_cast<uint8_t>(sample));
          sample >>= 8;
        }
      }
    }
  }

  return filtered_data;
}

}  // namespace microphone
}  // namespace esphome
