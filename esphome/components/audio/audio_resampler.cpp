#include "audio_resampler.h"

#ifdef USE_ESP32

#include "esphome/core/hal.h"

namespace esphome {
namespace audio {

static const uint32_t READ_WRITE_TIMEOUT_MS = 20;

AudioResampler::AudioResampler(size_t input_buffer_size, size_t output_buffer_size)
    : input_buffer_size_(input_buffer_size), output_buffer_size_(output_buffer_size) {
  this->input_transfer_buffer_ = AudioSourceTransferBuffer::create(input_buffer_size);
  this->output_transfer_buffer_ = AudioSinkTransferBuffer::create(output_buffer_size);
}

esp_err_t AudioResampler::add_source(std::weak_ptr<RingBuffer> &input_ring_buffer) {
  if (this->input_transfer_buffer_ != nullptr) {
    this->input_transfer_buffer_->set_source(input_ring_buffer);
    return ESP_OK;
  }
  return ESP_ERR_NO_MEM;
}

esp_err_t AudioResampler::add_sink(std::weak_ptr<RingBuffer> &output_ring_buffer) {
  if (this->output_transfer_buffer_ != nullptr) {
    this->output_transfer_buffer_->set_sink(output_ring_buffer);
    return ESP_OK;
  }
  return ESP_ERR_NO_MEM;
}

#ifdef USE_SPEAKER
esp_err_t AudioResampler::add_sink(speaker::Speaker *speaker) {
  if (this->output_transfer_buffer_ != nullptr) {
    this->output_transfer_buffer_->set_sink(speaker);
    return ESP_OK;
  }
  return ESP_ERR_NO_MEM;
}
#endif

esp_err_t AudioResampler::start(AudioStreamInfo &input_stream_info, AudioStreamInfo &output_stream_info,
                                uint16_t number_of_taps, uint16_t number_of_filters) {
  this->input_stream_info_ = input_stream_info;
  this->output_stream_info_ = output_stream_info;

  if ((this->input_transfer_buffer_ == nullptr) || (this->output_transfer_buffer_ == nullptr)) {
    return ESP_ERR_NO_MEM;
  }

  if ((input_stream_info.get_bits_per_sample() > 32) || (output_stream_info.get_bits_per_sample() > 32) ||
      (input_stream_info_.get_channels() != output_stream_info.get_channels())) {
    return ESP_ERR_NOT_SUPPORTED;
  }

  if ((input_stream_info.get_sample_rate() != output_stream_info.get_sample_rate()) ||
      (input_stream_info.get_bits_per_sample() != output_stream_info.get_bits_per_sample())) {
    this->resampler_ = make_unique<esp_audio_libs::resampler::Resampler>(
        input_stream_info.bytes_to_samples(this->input_buffer_size_),
        output_stream_info.bytes_to_samples(this->output_buffer_size_));

    // Use cascaded biquad filters when downsampling to avoid aliasing
    bool use_pre_filter = output_stream_info.get_sample_rate() < input_stream_info.get_sample_rate();

    esp_audio_libs::resampler::ResamplerConfiguration resample_config = {
        .source_sample_rate = static_cast<float>(input_stream_info.get_sample_rate()),
        .target_sample_rate = static_cast<float>(output_stream_info.get_sample_rate()),
        .source_bits_per_sample = input_stream_info.get_bits_per_sample(),
        .target_bits_per_sample = output_stream_info.get_bits_per_sample(),
        .channels = input_stream_info_.get_channels(),
        .use_pre_or_post_filter = use_pre_filter,
        .subsample_interpolate = false,  // Doubles the CPU load. Using more filters is a better alternative
        .number_of_taps = number_of_taps,
        .number_of_filters = number_of_filters,
    };

    if (!this->resampler_->initialize(resample_config)) {
      // Failed to allocate the resampler's internal buffers
      return ESP_ERR_NO_MEM;
    }
  }

  return ESP_OK;
}

AudioResamplerState AudioResampler::resample(bool stop_gracefully, int32_t *ms_differential) {
  if (stop_gracefully) {
    if (!this->input_transfer_buffer_->has_buffered_data() && (this->output_transfer_buffer_->available() == 0)) {
      return AudioResamplerState::FINISHED;
    }
  }

  if (!this->pause_output_) {
    // Move audio data to the sink
    this->output_transfer_buffer_->transfer_data_to_sink(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS));
  } else {
    // If paused, block to avoid wasting CPU resources
    delay(READ_WRITE_TIMEOUT_MS);
  }

  this->input_transfer_buffer_->transfer_data_from_source(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS));

  if (this->input_transfer_buffer_->available() == 0) {
    // No samples available to process
    return AudioResamplerState::RESAMPLING;
  }

  const size_t bytes_free = this->output_transfer_buffer_->free();
  const uint32_t frames_free = this->output_stream_info_.bytes_to_frames(bytes_free);

  const size_t bytes_available = this->input_transfer_buffer_->available();
  const uint32_t frames_available = this->input_stream_info_.bytes_to_frames(bytes_available);

  if ((this->input_stream_info_.get_sample_rate() != this->output_stream_info_.get_sample_rate()) ||
      (this->input_stream_info_.get_bits_per_sample() != this->output_stream_info_.get_bits_per_sample())) {
    esp_audio_libs::resampler::ResamplerResults results =
        this->resampler_->resample(this->input_transfer_buffer_->get_buffer_start(),
                                   this->output_transfer_buffer_->get_buffer_end(), frames_available, frames_free, -3);

    this->input_transfer_buffer_->decrease_buffer_length(this->input_stream_info_.frames_to_bytes(results.frames_used));
    this->output_transfer_buffer_->increase_buffer_length(
        this->output_stream_info_.frames_to_bytes(results.frames_generated));

    // Resampling causes slight differences in the durations used versus generated. Computes the difference in
    // millisconds. The callback function passing the played audio duration uses the difference to convert from output
    // duration to input duration.
    this->accumulated_frames_used_ += results.frames_used;
    this->accumulated_frames_generated_ += results.frames_generated;

    const int32_t used_ms =
        this->input_stream_info_.frames_to_milliseconds_with_remainder(&this->accumulated_frames_used_);
    const int32_t generated_ms =
        this->output_stream_info_.frames_to_milliseconds_with_remainder(&this->accumulated_frames_generated_);

    *ms_differential = used_ms - generated_ms;

  } else {
    // No resampling required, copy samples directly to the output transfer buffer
    *ms_differential = 0;

    const size_t bytes_to_transfer = std::min(this->output_stream_info_.frames_to_bytes(frames_free),
                                              this->input_stream_info_.frames_to_bytes(frames_available));

    std::memcpy((void *) this->output_transfer_buffer_->get_buffer_end(),
                (void *) this->input_transfer_buffer_->get_buffer_start(), bytes_to_transfer);

    this->input_transfer_buffer_->decrease_buffer_length(bytes_to_transfer);
    this->output_transfer_buffer_->increase_buffer_length(bytes_to_transfer);
  }

  return AudioResamplerState::RESAMPLING;
}

}  // namespace audio
}  // namespace esphome

#endif
