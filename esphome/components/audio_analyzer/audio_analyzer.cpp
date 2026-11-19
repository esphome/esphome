#include "audio_analyzer.h"

#ifdef USE_ESP32

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace esphome::audio_analyzer {

ESPHOME_LOG_TAG(TAG, "audio_analyzer");

static constexpr uint16_t FFT_SIZE = 512;
static constexpr uint8_t FFT_LOG2 = 9;
static constexpr float TARGET_ANALYSIS_RATE_HZ = 16000.0f;
static constexpr float FLOOR_DB = -100.0f;
static constexpr float FLOOR_POWER = 1e-10f;  // -100 dBFS

// Same as sound_level
static constexpr uint32_t MAX_FILL_DURATION_MS = 30;
static constexpr uint32_t RING_BUFFER_DURATION_MS = 120;

static inline float to_db(float power) { return 10.0f * log10f(std::max(power, FLOOR_POWER)); }

static inline float normalize(float value, float min_value, float max_value) {
  return clamp((value - min_value) / (max_value - min_value), 0.0f, 1.0f);
}

static inline float smoothing_alpha(float frame_seconds, uint32_t time_constant_ms) {
  if (time_constant_ms == 0) {
    return 1.0f;
  }
  return 1.0f - expf(-frame_seconds * 1000.0f / static_cast<float>(time_constant_ms));
}

// Reads channel 0 of a little-endian PCM frame as a float in [-1, 1)
static inline float read_sample(const uint8_t *p, uint8_t bytes_per_sample) {
  switch (bytes_per_sample) {
    case 2:
      return static_cast<float>(static_cast<int16_t>(p[0] | (p[1] << 8))) * (1.0f / 32768.0f);
    case 3:
      return static_cast<float>(static_cast<int32_t>((static_cast<uint32_t>(p[0]) << 8) |
                                                     (static_cast<uint32_t>(p[1]) << 16) |
                                                     (static_cast<uint32_t>(p[2]) << 24))) *
             (1.0f / 2147483648.0f);
    case 4:
      return static_cast<float>(
                 static_cast<int32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24))) *
             (1.0f / 2147483648.0f);
    case 1:
      return static_cast<float>(static_cast<int8_t>(p[0])) * (1.0f / 128.0f);
    default:
      return 0.0f;
  }
}

void AudioAnalyzer::dump_analysis_config_() {
  ESP_LOGCONFIG(TAG,
                "  Bands: %u log-spaced, %.0f-%.0f Hz\n"
                "  Power range: %.0f..%.0f dBFS\n"
                "  Band range: %.0f..%.0f dBFS\n"
                "  Attack/release: %" PRIu32 "/%" PRIu32 " ms",
                NUM_BANDS, this->min_frequency_, this->max_frequency_, this->power_min_db_, this->power_max_db_,
                this->band_min_db_, this->band_max_db_, this->attack_ms_, this->release_ms_);
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Power", this->power_sensor_);
  for (auto *band_sensor : this->band_sensors_) {
    LOG_SENSOR("  ", "Band", band_sensor);
  }
#endif
}

bool AudioAnalyzer::allocate_fft_() {
  if (this->fft_re_ != nullptr) {
    return true;
  }
  RAMAllocator<float> allocator;
  float *buffer = allocator.allocate(3 * FFT_SIZE + FFT_SIZE);  // re, im, window, cos/sin halves
  if (buffer == nullptr) {
    return false;
  }
  this->fft_re_ = buffer;
  this->fft_im_ = buffer + FFT_SIZE;
  this->window_ = buffer + 2 * FFT_SIZE;
  this->twiddle_cos_ = buffer + 3 * FFT_SIZE;
  this->twiddle_sin_ = this->twiddle_cos_ + FFT_SIZE / 2;

  this->window_power_sum_ = 0.0f;
  for (uint16_t i = 0; i < FFT_SIZE; ++i) {
    // Periodic Hann window
    const float w = 0.5f - 0.5f * cosf(2.0f * static_cast<float>(M_PI) * i / FFT_SIZE);
    this->window_[i] = w;
    this->window_power_sum_ += w * w;
  }
  for (uint16_t k = 0; k < FFT_SIZE / 2; ++k) {
    const float angle = 2.0f * static_cast<float>(M_PI) * k / FFT_SIZE;
    this->twiddle_cos_[k] = cosf(angle);
    this->twiddle_sin_[k] = sinf(angle);
  }
  this->band_db_.fill(FLOOR_DB);
  return true;
}

void AudioAnalyzer::configure_stream_(const audio::AudioStreamInfo &stream_info) {
  if (this->stream_configured_ && (stream_info == this->analysis_stream_info_)) {
    return;
  }
  this->analysis_stream_info_ = stream_info;
  this->stream_configured_ = true;

  const float sample_rate = static_cast<float>(stream_info.get_sample_rate());
  // Decimate by an integer factor to land near 16 kHz (48 kHz -> /3); keeps the FFT cost and resolution the same
  // for the microphone and the speaker.
  this->decimation_ = static_cast<uint8_t>(std::max(1L, lroundf(sample_rate / TARGET_ANALYSIS_RATE_HZ)));
  this->effective_sample_rate_ = sample_rate / this->decimation_;

  const float bin_width = this->effective_sample_rate_ / FFT_SIZE;
  const float ratio = this->max_frequency_ / this->min_frequency_;
  for (uint8_t b = 0; b <= NUM_BANDS; ++b) {
    const float edge_hz = this->min_frequency_ * powf(ratio, static_cast<float>(b) / NUM_BANDS);
    uint16_t bin = static_cast<uint16_t>(clamp<long>(lroundf(edge_hz / bin_width), 1, FFT_SIZE / 2));
    if (b > 0 && bin <= this->band_bins_[b - 1]) {
      bin = std::min<uint16_t>(this->band_bins_[b - 1] + 1, FFT_SIZE / 2);  // every band gets at least one bin
    }
    this->band_bins_[b] = bin;
  }
  ESP_LOGD(TAG, "Analyzing %" PRIu32 " Hz / %u bit / %u ch: decimation %u, %.1f Hz bins, %.1f ms frames",
           stream_info.get_sample_rate(), stream_info.get_bits_per_sample(), stream_info.get_channels(),
           this->decimation_, bin_width, 1000.0f * FFT_SIZE / this->effective_sample_rate_);
  this->reset_frame_();
}

void AudioAnalyzer::reset_frame_() {
  this->frame_fill_ = 0;
  this->decimation_sum_ = 0.0f;
  this->decimation_count_ = 0;
  this->squared_sum_ = 0.0f;
  this->squared_count_ = 0;
}

void AudioAnalyzer::process_frames_(const uint8_t *data, uint32_t frames) {
  const uint8_t bytes_per_sample = this->analysis_stream_info_.get_bits_per_sample() / 8;
  const size_t bytes_per_frame = this->analysis_stream_info_.frames_to_bytes(1);
  const float inv_decimation = 1.0f / this->decimation_;

  for (uint32_t i = 0; i < frames; ++i) {
    const float sample = read_sample(data + i * bytes_per_frame, bytes_per_sample);
    this->squared_sum_ += sample * sample;
    ++this->squared_count_;

    // Box-filter decimation: some aliasing, acceptable for coarse bands below 4 kHz
    this->decimation_sum_ += sample;
    if (++this->decimation_count_ < this->decimation_) {
      continue;
    }
    this->fft_re_[this->frame_fill_++] = this->decimation_sum_ * inv_decimation;
    this->decimation_sum_ = 0.0f;
    this->decimation_count_ = 0;

    if (this->frame_fill_ == FFT_SIZE) {
      this->analyze_frame_();
      this->frame_fill_ = 0;
    }
  }
}

void AudioAnalyzer::analyze_frame_() {
  float *re = this->fft_re_;
  float *im = this->fft_im_;

  for (uint16_t i = 0; i < FFT_SIZE; ++i) {
    re[i] *= this->window_[i];
    im[i] = 0.0f;
  }

  // In-place iterative radix-2 FFT
  for (uint16_t i = 1, j = 0; i < FFT_SIZE; ++i) {
    uint16_t bit = FFT_SIZE >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      std::swap(re[i], re[j]);
    }
  }
  for (uint8_t stage = 1; stage <= FFT_LOG2; ++stage) {
    const uint16_t len = 1 << stage;
    const uint16_t half = len >> 1;
    const uint16_t step = FFT_SIZE / len;
    for (uint16_t start = 0; start < FFT_SIZE; start += len) {
      for (uint16_t k = 0; k < half; ++k) {
        const float c = this->twiddle_cos_[k * step];
        const float s = this->twiddle_sin_[k * step];
        const uint16_t a = start + k;
        const uint16_t b = a + half;
        const float tr = re[b] * c + im[b] * s;
        const float ti = im[b] * c - re[b] * s;
        re[b] = re[a] - tr;
        im[b] = im[a] - ti;
        re[a] += tr;
        im[a] += ti;
      }
    }
  }

  // One-sided band power, normalized so the bands add up to the frame's mean square (Parseval)
  const float band_scale = 2.0f / (static_cast<float>(FFT_SIZE) * this->window_power_sum_);
  float band_db[NUM_BANDS];
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    float sum = 0.0f;
    for (uint16_t k = this->band_bins_[b]; k < this->band_bins_[b + 1]; ++k) {
      sum += re[k] * re[k] + im[k] * im[k];
    }
    band_db[b] = to_db(sum * band_scale);
  }

  const float power_db = to_db(this->squared_count_ > 0 ? this->squared_sum_ / this->squared_count_ : 0.0f);
  this->squared_sum_ = 0.0f;
  this->squared_count_ = 0;

  this->apply_targets_(power_db, band_db, FFT_SIZE / this->effective_sample_rate_);
  this->last_frame_ms_ = App.get_loop_component_start_time();
  this->active_ = true;
}

void AudioAnalyzer::apply_targets_(float power_db, const float *band_db, float frame_seconds) {
  const float attack = smoothing_alpha(frame_seconds, this->attack_ms_);
  const float release = smoothing_alpha(frame_seconds, this->release_ms_);

  this->power_db_ += (power_db - this->power_db_) * (power_db > this->power_db_ ? attack : release);
  this->power_level_ = normalize(this->power_db_, this->power_min_db_, this->power_max_db_);

  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    const float target_level = normalize(band_db[b], this->band_min_db_, this->band_max_db_);
    float &level = this->band_level_[b];
    level += (target_level - level) * (target_level > level ? attack : release);
    float &db = this->band_db_[b];
    db += (band_db[b] - db) * (band_db[b] > db ? attack : release);
  }

#ifdef USE_SENSOR
  this->power_db_hold_ = std::max(this->power_db_hold_, this->power_db_);
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    this->band_level_hold_[b] = std::max(this->band_level_hold_[b], this->band_level_[b]);
  }
#endif
}

void AudioAnalyzer::decay_if_idle_() {
  const uint32_t frame_ms =
      static_cast<uint32_t>(1000.0f * FFT_SIZE / (this->stream_configured_ ? this->effective_sample_rate_ : 16000.0f));
  const uint32_t now = App.get_loop_component_start_time();
  if ((now - this->last_frame_ms_) < 2 * frame_ms) {
    return;
  }
  this->active_ = false;
  if (this->power_db_ <= FLOOR_DB + 0.1f && this->power_level_ <= 0.0f &&
      *std::max_element(this->band_level_.begin(), this->band_level_.end()) < 0.001f) {
    return;  // fully released
  }
  float silence[NUM_BANDS];
  std::fill(silence, silence + NUM_BANDS, FLOOR_DB);
  this->apply_targets_(FLOOR_DB, silence, frame_ms / 1000.0f);
  this->last_frame_ms_ = now - frame_ms;  // next release step one frame from now
}

void AudioAnalyzer::update() {
#ifdef USE_SENSOR
  PUBLISH_STATE(this->power_sensor_, this->power_db_hold_);
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    PUBLISH_STATE(this->band_sensors_[b], this->band_level_hold_[b]);
  }
  this->power_db_hold_ = this->power_db_;
  this->band_level_hold_ = this->band_level_;
#endif
}

void MicrophoneAudioAnalyzer::dump_config() {
  ESP_LOGCONFIG(TAG, "Audio Analyzer:");
  this->dump_analysis_config_();
}

void MicrophoneAudioAnalyzer::setup() {
  if (!this->allocate_fft_()) {
    this->mark_failed(LOG_STR("Failed to allocate FFT buffers"));
    return;
  }

  this->microphone_source_->add_data_callback([this](const microphone::AudioCapture &capture) {
    std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = this->ring_buffer_.lock();
    if (temp_ring_buffer != nullptr) {
      temp_ring_buffer->write((void *) capture.data.data(), capture.data.size());
    }
  });

  if (!this->microphone_source_->is_passive()) {
    // Automatically start the microphone if not in passive mode
    this->microphone_source_->start();
  }
}

void MicrophoneAudioAnalyzer::loop() {
  if (this->microphone_source_->is_running() && !this->status_has_error()) {
    // Allocate buffers
    if (this->start_()) {
      this->status_clear_warning();
    }
  } else {
    if (!this->status_has_warning()) {
      this->status_set_warning(LOG_STR("Microphone isn't running, can't analyze audio"));
      // Deallocate buffers, if necessary
      this->stop_();
      this->reset_frame_();
    }
    this->decay_if_idle_();
    return;
  }

  if (this->status_has_error()) {
    return;
  }

  this->configure_stream_(this->microphone_source_->get_audio_stream_info());

  // Expose a chunk of the ring buffer's internal storage - don't block to avoid slowing the main loop.
  this->audio_source_->fill(0);
  const size_t available = this->audio_source_->available();
  if (available > 0) {
    const uint32_t frames = this->analysis_stream_info_.bytes_to_frames(available);
    this->process_frames_(this->audio_source_->data(), frames);
    this->audio_source_->consume(this->analysis_stream_info_.frames_to_bytes(frames));
  }

  this->decay_if_idle_();
}

bool MicrophoneAudioAnalyzer::start_() {
  if (this->audio_source_ != nullptr) {
    return true;
  }

  const auto stream_info = this->microphone_source_->get_audio_stream_info();
  const size_t bytes_per_frame = stream_info.frames_to_bytes(1);

  this->ring_buffer_.reset();
  const size_t ring_buffer_size =
      (stream_info.ms_to_bytes(RING_BUFFER_DURATION_MS) / bytes_per_frame) * bytes_per_frame;
  std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = ring_buffer::RingBuffer::create(ring_buffer_size);
  if (temp_ring_buffer == nullptr) {
    this->status_momentary_error(15000);
    return false;
  }

  this->audio_source_ = audio::RingBufferAudioSource::create(
      temp_ring_buffer, stream_info.ms_to_bytes(MAX_FILL_DURATION_MS), static_cast<uint8_t>(bytes_per_frame));
  if (this->audio_source_ == nullptr) {
    this->status_momentary_error(15000);
    return false;
  }
  this->ring_buffer_ = temp_ring_buffer;

  this->status_clear_error();
  return true;
}

void MicrophoneAudioAnalyzer::stop_() { this->audio_source_.reset(); }

}  // namespace esphome::audio_analyzer

#endif
