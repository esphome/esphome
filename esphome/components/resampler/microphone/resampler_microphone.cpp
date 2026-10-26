#include "resampler_microphone.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::resampler {

ESPHOME_LOG_TAG(TAG, "resampler.microphone");

// Duration of audio the resampler converts per step; longer source chunks are processed in several steps
static constexpr uint32_t BUFFER_DURATION_MS = 16;

void ResamplerMicrophone::setup() {
  const audio::AudioStreamInfo input_stream_info = this->source_->get_audio_stream_info();
  this->audio_stream_info_ = audio::AudioStreamInfo(input_stream_info.get_bits_per_sample(),
                                                    input_stream_info.get_channels(), this->target_sample_rate_);

  // Allocate now for the expected source format; process_audio_ only sets up again if that format changes
  if (!this->init_resampler_(input_stream_info)) {
    this->mark_failed();
    return;
  }

  this->source_->add_data_callback([this](const std::vector<uint8_t> &data) { this->process_audio_(data); });

  this->disable_loop();
}

void ResamplerMicrophone::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Resampler Microphone:\n"
                "  Target Sample Rate: %" PRIu32 " Hz\n"
                "  Taps: %u\n"
                "  Filters: %u",
                this->target_sample_rate_, this->taps_, this->filters_);
#ifdef USE_DSP_DECIMATOR
  ESP_LOGCONFIG(TAG, "  ESP-DSP integer-ratio decimation: %s", YESNO(this->fast_decimation_));
#endif
}

void ResamplerMicrophone::start() {
  if (this->is_failed() || this->active_listeners_ == UINT8_MAX)
    return;
  ++this->active_listeners_;
  this->enable_loop();
}

void ResamplerMicrophone::stop() {
  if (this->active_listeners_ == 0)
    return;
  --this->active_listeners_;
  this->enable_loop();
}

void ResamplerMicrophone::loop() {
  if (this->active_listeners_ == 0) {
    if (this->state_ != microphone::STATE_STOPPED) {
      this->source_->stop();
      this->state_ = microphone::STATE_STOPPED;
    }
    this->disable_loop();
    return;
  }

  switch (this->state_) {
    case microphone::STATE_STOPPED:
#ifdef USE_DSP_DECIMATOR
      if (this->fast_decimation_) {
        this->dsp_decimator_.request_reset();
      }
#endif
      this->source_->start();
      this->state_ = microphone::STATE_STARTING;
      break;
    case microphone::STATE_STARTING:
      if (this->source_->is_running()) {
        this->state_ = microphone::STATE_RUNNING;
      }
      break;
    case microphone::STATE_RUNNING:
      // Follow the source if it restarts, e.g. after a driver error
      if (!this->source_->is_running()) {
        this->state_ = microphone::STATE_STARTING;
      }
      break;
    case microphone::STATE_STOPPING:
      break;
  }
}

bool ResamplerMicrophone::init_resampler_(const audio::AudioStreamInfo &input_stream_info) {
  this->resampler_.reset();
  this->resampler_ready_ = false;

  if (input_stream_info.get_sample_rate() == this->target_sample_rate_) {
    // The source already delivers the target sample rate, so its audio is passed through unchanged
    this->input_stream_info_ = input_stream_info;
    this->resampler_ready_ = true;
    return true;
  }

#ifdef USE_DSP_DECIMATOR
  if (this->fast_decimation_) {
    const uint32_t source_rate = input_stream_info.get_sample_rate();
    if (source_rate <= this->target_sample_rate_ || source_rate % this->target_sample_rate_ != 0 ||
        source_rate / this->target_sample_rate_ < audio::MIN_DSP_DECIMATION_RATIO ||
        source_rate / this->target_sample_rate_ > audio::MAX_DSP_DECIMATION_RATIO ||
        input_stream_info.get_bits_per_sample() != 16 || input_stream_info.get_channels() != 1) {
      ESP_LOGE(TAG, "Fast decimation requires mono 16-bit input and an integer rate ratio from 2:1 to 16:1");
      return false;
    }
    const uint32_t ratio = source_rate / this->target_sample_rate_;
    if (!this->dsp_decimator_.configure(source_rate, this->target_sample_rate_)) {
      ESP_LOGE(TAG, "ESP-DSP decimator initialization failed");
      return false;
    }
    const uint32_t max_input_frames = input_stream_info.ms_to_frames(BUFFER_DURATION_MS);
    this->output_buffer_.reserve(this->audio_stream_info_.frames_to_bytes((max_input_frames + ratio - 1) / ratio));
    this->input_stream_info_ = input_stream_info;
    this->resampler_ready_ = true;
    return true;
  }
#endif

  const audio::AudioStreamInfo output_stream_info(input_stream_info.get_bits_per_sample(),
                                                  input_stream_info.get_channels(), this->target_sample_rate_);

  auto resampler = make_unique<esp_audio_libs::resampler::Resampler>(
      input_stream_info.ms_to_samples(BUFFER_DURATION_MS), output_stream_info.ms_to_samples(BUFFER_DURATION_MS));

  esp_audio_libs::resampler::ResamplerConfiguration resample_config = {
      .source_sample_rate = static_cast<float>(input_stream_info.get_sample_rate()),
      .target_sample_rate = static_cast<float>(this->target_sample_rate_),
      .source_bits_per_sample = input_stream_info.get_bits_per_sample(),
      .target_bits_per_sample = input_stream_info.get_bits_per_sample(),
      .channels = input_stream_info.get_channels(),
      // Filters out frequencies above the new Nyquist limit when downsampling, to avoid aliasing
      .use_pre_or_post_filter = this->target_sample_rate_ < input_stream_info.get_sample_rate(),
      .subsample_interpolate = false,  // Doubles the CPU load; more filters is a better alternative
      .number_of_taps = this->taps_,
      .number_of_filters = this->filters_,
  };

  if (!resampler->initialize(resample_config)) {
    ESP_LOGE(TAG, "Not enough memory to resample");
    return false;
  }

  this->output_buffer_.reserve(output_stream_info.ms_to_bytes(BUFFER_DURATION_MS));
  this->resampler_ = std::move(resampler);
  // Only set on success, so a failed set up is retried with the next chunk
  this->input_stream_info_ = input_stream_info;
  this->resampler_ready_ = true;
  return true;
}

void ResamplerMicrophone::process_audio_(const std::vector<uint8_t> &data) {
  const audio::AudioStreamInfo input_stream_info = this->source_->get_audio_stream_info();
  if (input_stream_info != this->input_stream_info_) {
    this->init_resampler_(input_stream_info);
  }
  if (!this->resampler_ready_) {
    return;
  }

#ifdef USE_DSP_DECIMATOR
  if (this->fast_decimation_ && input_stream_info.get_sample_rate() != this->target_sample_rate_) {
    // Keep the callback output within its setup-time reservation.
    const size_t max_input_bytes = input_stream_info.ms_to_bytes(BUFFER_DURATION_MS);
    for (size_t offset = 0; offset < data.size(); offset += max_input_bytes) {
      const size_t bytes = std::min(max_input_bytes, data.size() - offset);
      if (!this->dsp_decimator_.process(data.data() + offset, bytes, this->output_buffer_)) {
        ESP_LOGE(TAG, "ESP-DSP decimator reset failed");
        return;
      }
      if (!this->output_buffer_.empty())
        this->data_callbacks_.call(this->output_buffer_);
    }
    return;
  }
#endif

  if (this->resampler_ == nullptr) {
    this->data_callbacks_.call(data);
    return;
  }

  const size_t input_bytes_per_frame = input_stream_info.frames_to_bytes(1);
  const uint32_t max_input_frames = input_stream_info.ms_to_frames(BUFFER_DURATION_MS);
  // Both limits match the sizes the resampler's internal buffers were allocated with in init_resampler_
  const uint32_t max_output_frames = this->audio_stream_info_.ms_to_frames(BUFFER_DURATION_MS);

  const uint8_t *input = data.data();
  uint32_t input_frames = input_stream_info.bytes_to_frames(data.size());
  while (input_frames > 0) {
    // Stays within the reserved capacity, so this never reallocates
    this->output_buffer_.resize(this->audio_stream_info_.frames_to_bytes(max_output_frames));

    // The resampler's internal buffers hold at most BUFFER_DURATION_MS of audio, so feed it in steps of that size.
    // 0 dB keeps the microphone level that downstream detectors are tuned for; overshoot saturates instead of wrapping.
    esp_audio_libs::resampler::ResamplerResults results = this->resampler_->resample(
        input, this->output_buffer_.data(), std::min(input_frames, max_input_frames), max_output_frames, 0.0f);

    input += results.frames_used * input_bytes_per_frame;
    input_frames -= results.frames_used;

    if (results.frames_generated > 0) {
      this->output_buffer_.resize(this->audio_stream_info_.frames_to_bytes(results.frames_generated));
      this->data_callbacks_.call(this->output_buffer_);
    } else if (results.frames_used == 0) {
      break;  // No progress; drop the rest of the chunk instead of spinning
    }
  }
}

}  // namespace esphome::resampler

#endif  // USE_ESP32
