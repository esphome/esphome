#include "i2s_aec_microphone.h"

#if defined(USE_ESP32) && defined(USE_ESP_IDF) && defined(USE_I2S_AEC_MICROPHONE)

#include "esphome/core/log.h"
#ifdef USE_I2S_AEC_MICROPHONE_DIAGNOSTICS
#include "reference_probe.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

namespace esphome::i2s_aec_microphone {

ESPHOME_LOG_TAG(TAG, "i2s_aec_microphone");
DRAM_ATTR I2SAECMicrophone::TxBlock I2SAECMicrophone::tx_queue[TX_QUEUE];
DRAM_ATTR StackType_t I2SAECMicrophone::task_stack[4096];
StaticTask_t I2SAECMicrophone::task_buffer;
alignas(16) int16_t I2SAECMicrophone::mic_frame[FRAME];
alignas(16) int16_t I2SAECMicrophone::ref_frame[FRAME];
alignas(16) int16_t I2SAECMicrophone::out_frame[FRAME];

void I2SAECMicrophone::setup() {
  this->audio_stream_info_ = audio::AudioStreamInfo(16, 1, RATE);
  if (this->source_ == nullptr || this->bus_ == nullptr || !this->bus_->is_full_duplex() || this->bus_->is_failed() ||
      (this->reference_sample_rate_ != RATE && this->reference_sample_rate_ != FAST_REFERENCE_RATIO * RATE)) {
    this->mark_failed(LOG_STR("A full-duplex I2S bus is required"));
    return;
  }
  this->mic_ring_ = static_cast<int16_t *>(heap_caps_malloc(RING * sizeof(int16_t), MALLOC_CAP_SPIRAM));
  this->ref_ring_ = static_cast<int16_t *>(heap_caps_malloc(RING * sizeof(int16_t), MALLOC_CAP_SPIRAM));
  if (this->mic_ring_ == nullptr || this->ref_ring_ == nullptr) {
    heap_caps_free(this->mic_ring_);
    heap_caps_free(this->ref_ring_);
    this->mic_ring_ = nullptr;
    this->ref_ring_ = nullptr;
    this->mark_failed(LOG_STR("AEC PSRAM ring allocation failed"));
    return;
  }
  memset(this->mic_ring_, 0, RING * sizeof(int16_t));
  memset(this->ref_ring_, 0, RING * sizeof(int16_t));
  if (this->reference_sample_rate_ == FAST_REFERENCE_RATIO * RATE)
    this->setup_reference_converter_();
  if (this->is_failed()) {
    heap_caps_free(this->mic_ring_);
    heap_caps_free(this->ref_ring_);
    this->mic_ring_ = nullptr;
    this->ref_ring_ = nullptr;
    return;
  }
  this->output_.resize(FRAME * sizeof(int16_t));
  this->task_ = xTaskCreateStaticPinnedToCore(&I2SAECMicrophone::task_entry, "aec_live", sizeof(task_stack), this, 5,
                                              task_stack, &task_buffer, this->task_core_);
  if (this->task_ == nullptr) {
    heap_caps_free(this->mic_ring_);
    heap_caps_free(this->ref_ring_);
    this->mic_ring_ = nullptr;
    this->ref_ring_ = nullptr;
    this->mark_failed(LOG_STR("AEC task creation failed"));
    return;
  }
  this->source_->add_data_callback([this](const std::vector<uint8_t> &data) { this->on_microphone_(data); });
  this->bus_->set_tx_dma_observer(&I2SAECMicrophone::on_tx_dma, this);
}

void I2SAECMicrophone::setup_reference_converter_() {
#ifdef USE_I2S_AEC_MICROPHONE_DSP_DECIMATOR
  if (this->fast_reference_decimation_) {
    if (!this->reference_decimator_.configure(this->reference_sample_rate_, RATE))
      this->mark_failed(LOG_STR("AEC reference decimator initialization failed"));
    return;
  }
#endif
  auto resampler = std::make_unique<esp_audio_libs::resampler::Resampler>(MAX_TX_FRAMES, FRAME);
  esp_audio_libs::resampler::ResamplerConfiguration config = {
      .source_sample_rate = static_cast<float>(FAST_REFERENCE_RATIO * RATE),
      .target_sample_rate = static_cast<float>(RATE),
      .source_bits_per_sample = 16,
      .target_bits_per_sample = 16,
      .channels = 1,
      .use_pre_or_post_filter = true,
      .subsample_interpolate = false,
      .number_of_taps = 16,
      .number_of_filters = 16,
  };
  if (!resampler->initialize(config)) {
    this->mark_failed(LOG_STR("AEC reference resampler initialization failed"));
  } else {
    this->reference_resampler_ = std::move(resampler);
    this->reference_output_.resize(FRAME * sizeof(int16_t));
  }
}

void I2SAECMicrophone::dump_config() {
  ESP_LOGCONFIG(TAG, "Live ESP-SR AEC: %" PRIu32 " -> 16 kHz, filter=%d, core=%d, ready=%s",
                this->reference_sample_rate_, this->filter_length_, this->task_core_, YESNO(!this->is_failed()));
#ifdef USE_I2S_AEC_MICROPHONE_DSP_DECIMATOR
  ESP_LOGCONFIG(TAG, "  Fast reference decimation: %s", YESNO(this->fast_reference_decimation_));
#endif
#ifdef USE_I2S_AEC_MICROPHONE_DIAGNOSTICS
  ESP_LOGCONFIG(TAG, "  AEC diagnostics: compiled in; output follows this component's DEBUG log level");
#endif
}

void I2SAECMicrophone::start() {
  if (this->is_failed() || this->listeners_ == UINT8_MAX)
    return;
  if (this->listeners_.fetch_add(1) == 0) {
#ifdef USE_I2S_AEC_MICROPHONE_DSP_DECIMATOR
    if (this->fast_reference_decimation_)
      this->reference_decimator_.request_reset();
#endif
    this->mic_written_.store(0);
    this->synchronized_.store(false);
    this->generation_.fetch_add(1);
    this->source_->start();
    this->state_ = microphone::STATE_STARTING;
  }
}

void I2SAECMicrophone::stop() {
  if (this->listeners_ == 0)
    return;
  if (this->listeners_.fetch_sub(1) == 1) {
    this->state_ = microphone::STATE_STOPPED;
    this->generation_.fetch_add(1);
    this->source_->stop();
  }
}

void I2SAECMicrophone::loop() {
  if (this->state_ == microphone::STATE_STARTING && this->source_->is_running())
    this->state_ = microphone::STATE_RUNNING;
  if (!this->aec_attempted_ && this->listeners_.load() != 0) {
    this->aec_attempted_ = true;
    auto *handle = aec_create(RATE, this->filter_length_, 1, AEC_MODE_SR_LOW_COST);
    if (handle != nullptr && aec_get_chunksize(handle) == FRAME) {
      this->aec_.store(handle, std::memory_order_release);
      ESP_LOGI(TAG, "ESP-SR AEC initialized");
    } else {
      if (handle != nullptr)
        aec_destroy(handle);
      ESP_LOGE(TAG, "ESP-SR AEC initialization failed; microphone remains unprocessed");
      this->status_set_warning(LOG_STR("AEC initialization failed; audio is unprocessed"));
    }
  }
}

void IRAM_ATTR I2SAECMicrophone::on_tx_dma(const uint8_t *data, size_t bytes, void *context) {
  auto *self = static_cast<I2SAECMicrophone *>(context);
  if (self->listeners_.load(std::memory_order_relaxed) == 0)
    return;
  const size_t frame_bytes = 2 * sizeof(int16_t);
  const size_t ratio = self->reference_sample_rate_ / RATE;
  if (bytes == 0 || bytes > MAX_TX_FRAMES * frame_bytes || bytes % (ratio * frame_bytes) != 0) {
    self->tx_drops_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  const size_t frames = bytes / frame_bytes;
  const auto *stereo = reinterpret_cast<const int16_t *>(data);
  const uint32_t first = self->tx_total_.load(std::memory_order_relaxed);
  const uint32_t produced = self->tx_produced_.load(std::memory_order_relaxed);
  bool nonzero_audio = false;
  if (produced - self->tx_consumed_.load(std::memory_order_acquire) >= TX_QUEUE) {
    self->tx_drops_.fetch_add(1, std::memory_order_relaxed);
  } else {
    auto &block = tx_queue[produced % TX_QUEUE];
    block.first = first;
    block.count = frames;
    for (size_t i = 0; i < block.count; ++i) {
      block.mono[i] = static_cast<int16_t>((static_cast<int32_t>(stereo[2 * i]) + stereo[2 * i + 1]) / 2);
      nonzero_audio |= stereo[2 * i] != 0 || stereo[2 * i + 1] != 0;
    }
    self->tx_produced_.store(produced + 1, std::memory_order_release);
  }
  if (nonzero_audio)
    self->tx_last_us_.store(static_cast<uint32_t>(esp_timer_get_time()), std::memory_order_relaxed);
  self->tx_total_.store(first + frames / ratio, std::memory_order_release);
}

void I2SAECMicrophone::on_microphone_(const std::vector<uint8_t> &data) {
  if (this->listeners_.load(std::memory_order_relaxed) == 0 || this->mic_ring_ == nullptr || data.size() % 2 != 0)
    return;
  const size_t count = data.size() / sizeof(int16_t);
  const uint32_t first = this->mic_written_.load(std::memory_order_relaxed);
  const uint32_t tx_completed = this->tx_total_.load(std::memory_order_acquire);
  const uint32_t tx_time = this->tx_last_us_.load(std::memory_order_relaxed);
  const uint32_t age_us = static_cast<uint32_t>(esp_timer_get_time()) - tx_time;
  constexpr uint32_t tx_idle_timeout_us = 3 * FRAME * 1000000 / RATE;
  const bool active_tx = tx_time != 0 && age_us < tx_idle_timeout_us;
  if (!active_tx) {
    this->tx_was_active_ = false;
    this->synchronized_.store(false, std::memory_order_release);
  } else {
    // Use completed TX DMA samples, not the resampled reference's ready count:
    // the latter can lag by queued blocks when processing takes longer.
    const int32_t observed = static_cast<int32_t>(tx_completed - (first + count));
    static constexpr int32_t PHASE_FILTER_SCALE = 32;
    const int64_t observed_q5 = static_cast<int64_t>(observed) * PHASE_FILTER_SCALE;
    if (!this->tx_was_active_) {
      this->observed_base_filter_q5_ = observed_q5;
    } else {
      // Preserve fractional samples: integer division at every callback would
      // leave a permanent error of up to 31 samples after a phase step.
      this->observed_base_filter_q5_ += (observed_q5 - this->observed_base_filter_q5_) / PHASE_FILTER_SCALE;
    }
    this->observed_base_.store(static_cast<int32_t>(this->observed_base_filter_q5_ / PHASE_FILTER_SCALE),
                               std::memory_order_release);
    this->tx_was_active_ = true;
  }
  this->drain_tx_();
  const auto *samples = reinterpret_cast<const int16_t *>(data.data());
  for (size_t i = 0; i < count; ++i)
    this->mic_ring_[(first + i) % RING] = samples[i];
  this->mic_written_.store(first + count, std::memory_order_release);
  // Drain the ISR-produced reference while the microphone task is runnable. The
  // AEC task can spend over 20 ms inside aec_process(), long enough to overflow
  // the three-block TX queue.
  if (this->task_ != nullptr)
    xTaskNotifyGive(this->task_);
}

bool I2SAECMicrophone::calibrate_reference_(uint32_t mic_write) {
  const uint32_t ready = this->ref_ready_.load(std::memory_order_acquire);
  if (mic_write < 3 * FRAME || ready < RING)
    return false;

  // Check two consecutive, completed microphone frames against the completed
  // TX-DMA history. Correlation is only evidence that playback reached this
  // microphone; periodic music must not choose the sample-clock origin.
  const uint32_t mic_start = mic_write - 3 * FRAME;
  constexpr uint32_t stride = 8;
  constexpr uint32_t points = FRAME / stride;
  constexpr double minimum_correlation_squared = 0.49;  // |r| >= 0.7 in both frames
  double best_score = 0;
  double best_frame_scores[2]{};
#ifdef USE_I2S_AEC_MICROPHONE_DIAGNOSTICS
  uint32_t best_ref_start = 0;
#endif
  for (uint32_t ref_start = ready - RING + FRAME; ref_start <= ready - 2 * FRAME - FRAME / 4; ref_start += stride) {
    double scores[2]{};
    for (uint32_t frame = 0; frame < 2; ++frame) {
      int64_t sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0, sum_yy = 0;
      for (uint32_t point = 0; point < points; ++point) {
        const int32_t x = this->mic_ring_[(mic_start + frame * FRAME + point * stride) % RING];
        const int32_t y = this->ref_ring_[(ref_start + frame * FRAME + point * stride) % RING];
        sum_x += x;
        sum_y += y;
        sum_xy += static_cast<int64_t>(x) * y;
        sum_xx += static_cast<int64_t>(x) * x;
        sum_yy += static_cast<int64_t>(y) * y;
      }
      const double variance_x = static_cast<double>(points * sum_xx - sum_x * sum_x);
      const double variance_y = static_cast<double>(points * sum_yy - sum_y * sum_y);
      if (variance_x < 100.0 * 100.0 * points * points || variance_y < 50.0 * 50.0 * points * points)
        break;
      const double covariance = static_cast<double>(points * sum_xy - sum_x * sum_y);
      scores[frame] = covariance * covariance / (variance_x * variance_y);
    }
    const double score = scores[0] + scores[1];
    if (score > best_score) {
      best_score = score;
      best_frame_scores[0] = scores[0];
      best_frame_scores[1] = scores[1];
#ifdef USE_I2S_AEC_MICROPHONE_DIAGNOSTICS
      best_ref_start = ref_start;
#endif
    }
  }
  if (best_frame_scores[0] < minimum_correlation_squared || best_frame_scores[1] < minimum_correlation_squared)
    return false;

  // Both this origin and the phase monitor come from the same callback-sampled
  // clock difference. Sampling the counters here in the AEC task instead would
  // add variable worker and resampler scheduling delay.
  const int32_t counter_base = this->observed_base_.load(std::memory_order_acquire);
  this->mic_ref_base_.store(static_cast<uint32_t>(counter_base), std::memory_order_relaxed);
  this->synchronized_.store(true, std::memory_order_release);
#ifdef USE_I2S_AEC_MICROPHONE_DIAGNOSTICS
  const int32_t correlation_base = static_cast<int32_t>(best_ref_start - mic_start);
  this->probe_lag_initial_ = counter_base - correlation_base;
  this->probe_lag_initialized_ = true;
  ++this->probe_lag_revision_;
  ESP_LOGD(TAG,
           "AEC calibration: correlation r2=%.2f/%.2f, best base=%" PRId32 ", counter base=%" PRId32
           ", lag candidate=%" PRId32 " samples",
           best_frame_scores[0], best_frame_scores[1], correlation_base, counter_base, this->probe_lag_initial_);
#endif
  ESP_LOGI(TAG, "AEC reference synchronized");
  return true;
}

void I2SAECMicrophone::task_entry(void *context) { static_cast<I2SAECMicrophone *>(context)->run_(); }

size_t I2SAECMicrophone::convert_reference_(const TxBlock &block, const uint8_t *&samples) {
  if (this->reference_sample_rate_ == RATE)
    return block.count;
#ifdef USE_I2S_AEC_MICROPHONE_DSP_DECIMATOR
  if (this->fast_reference_decimation_) {
    size_t generated = 0;
    const bool ok = this->reference_decimator_.process(block.mono, block.count, this->decimated_reference_.data(),
                                                       this->decimated_reference_.size(), generated);
    samples = reinterpret_cast<const uint8_t *>(this->decimated_reference_.data());
    return ok ? generated : 0;
  }
#endif
  const auto result = this->reference_resampler_->resample(samples, this->reference_output_.data(), block.count,
                                                           this->reference_output_.size() / sizeof(int16_t), 0.0f);
  samples = this->reference_output_.data();
  return result.frames_used == block.count ? result.frames_generated : 0;
}

void I2SAECMicrophone::drain_tx_() {
  uint32_t consumed = this->tx_consumed_.load(std::memory_order_relaxed);
  const uint32_t produced = this->tx_produced_.load(std::memory_order_acquire);
  while (consumed != produced) {
    const auto &block = tx_queue[consumed % TX_QUEUE];
    const uint32_t previous_ready = this->ref_ready_.load(std::memory_order_relaxed);
    const uint32_t gap = block.first - previous_ready;
    if (block.first > previous_ready) {
      const uint32_t fill_start = gap > RING ? block.first - RING : previous_ready;
      for (uint32_t sample = fill_start; sample < block.first; ++sample)
        this->ref_ring_[sample % RING] = 0;
    }
    const size_t expected = block.count * RATE / this->reference_sample_rate_;
    const uint8_t *samples = reinterpret_cast<const uint8_t *>(block.mono);
    const size_t count = this->convert_reference_(block, samples);
    if (count != expected)
      this->tx_drops_.fetch_add(1, std::memory_order_relaxed);
    for (size_t i = 0; i < expected; ++i) {
      const int16_t sample = count == expected ? static_cast<int16_t>(static_cast<uint16_t>(samples[2 * i]) |
                                                                      (static_cast<uint16_t>(samples[2 * i + 1]) << 8))
                                               : 0;
      this->ref_ring_[(block.first + i) % RING] = sample;
    }
    this->ref_ready_.store(block.first + expected, std::memory_order_release);
    this->tx_consumed_.store(++consumed, std::memory_order_release);
  }
}

void I2SAECMicrophone::run_() {
  uint32_t current_generation = this->generation_.load();
  uint32_t last_tx_drops = 0;
#ifdef USE_I2S_AEC_MICROPHONE_DIAGNOSTICS
  ReferenceProbe probe;
  // Cache only the sparse reference samples for the current frame. The TX
  // producer can advance the PSRAM ring while aec_process() is running.
  std::array<std::array<int16_t, ReferenceProbe::LAG_COUNT>, FRAME / ReferenceProbe::SAMPLE_STRIDE> probe_reference{};
  int32_t probe_lag = 0;
  int32_t pending_lag = 0;
  uint8_t pending_lag_windows = 0;
  uint32_t probe_lag_revision = 0;
  uint16_t probe_frames = 0;
  uint16_t probe_idle_frames = 0;
  int32_t probe_phase_min = 0;
  int32_t probe_phase_max = 0;
  uint32_t probe_mic_drops = this->mic_drops_.load(std::memory_order_relaxed);
  uint32_t probe_tx_drops = this->tx_drops_.load(std::memory_order_relaxed);
  auto reset_probe_window = [&]() {
    probe.reset();
    probe_frames = 0;
    probe_phase_min = 0;
    probe_phase_max = 0;
    probe_mic_drops = this->mic_drops_.load(std::memory_order_relaxed);
    probe_tx_drops = this->tx_drops_.load(std::memory_order_relaxed);
  };
#endif
  while (true) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
    if (this->ref_ring_ == nullptr)
      continue;
    const uint32_t generation = this->generation_.load();
    if (generation != current_generation) {
      current_generation = generation;
      this->mic_read_ = 0;
      this->last_calibration_mic_ = 0;
      this->silent_frames_ = 0;
#ifdef USE_I2S_AEC_MICROPHONE_DIAGNOSTICS
      reset_probe_window();
      probe_idle_frames = 0;
      this->probe_lag_initialized_ = false;
      pending_lag_windows = 0;
#endif
    }
    if (this->listeners_ == 0)
      continue;
    const uint32_t mic_write_for_calibration = this->mic_written_.load(std::memory_order_acquire);
    const uint32_t last_audio_us = this->tx_last_us_.load(std::memory_order_acquire);
    const bool audible_tx =
        last_audio_us != 0 && static_cast<uint32_t>(esp_timer_get_time()) - last_audio_us < 3 * FRAME * 1000000 / RATE;
    if (!this->synchronized_.load(std::memory_order_acquire) && audible_tx &&
        mic_write_for_calibration - this->last_calibration_mic_ >= RATE) {
      this->last_calibration_mic_ = mic_write_for_calibration;
      this->calibrate_reference_(mic_write_for_calibration);
    } else if (this->synchronized_.load(std::memory_order_acquire) &&
               mic_write_for_calibration - this->last_calibration_mic_ >= RATE) {
      this->last_calibration_mic_ = mic_write_for_calibration;
      const int32_t observed = this->observed_base_.load(std::memory_order_acquire);
      const int32_t current = static_cast<int32_t>(this->mic_ref_base_.load(std::memory_order_relaxed));
      // A lost fraction of an RX DMA buffer can shift the microphones without
      // overflowing the DMA queue. Two milliseconds are enough to invalidate
      // the current acoustic alignment, even though a full frame was not lost.
      if (std::abs(observed - current) > static_cast<int32_t>(FRAME / 16)) {
        this->synchronized_.store(false, std::memory_order_release);
        ESP_LOGW(TAG,
                 "AEC reference phase changed by %" PRId32 " samples (observed=%" PRId32 ", base=%" PRId32
                 ", TX queued=%" PRIu32 ", RX=%" PRIu32 "); recalibrating",
                 observed - current, observed, current,
                 this->tx_total_.load(std::memory_order_relaxed) - this->ref_ready_.load(std::memory_order_relaxed),
                 mic_write_for_calibration);
      }
    }
#ifdef USE_I2S_AEC_MICROPHONE_DIAGNOSTICS
    if (this->probe_lag_initialized_ && probe_lag_revision != this->probe_lag_revision_) {
      reset_probe_window();
      probe_lag = this->probe_lag_initial_;
      probe_lag_revision = this->probe_lag_revision_;
      pending_lag_windows = 0;
    }
#endif
    while (this->mic_written_.load(std::memory_order_acquire) - this->mic_read_ >= FRAME) {
      const uint32_t mic_write = this->mic_written_.load(std::memory_order_acquire);
      if (mic_write - this->mic_read_ > RING) {
        this->mic_read_ = mic_write - FRAME;
        this->mic_drops_.fetch_add(1, std::memory_order_relaxed);
      }
      const uint32_t ref_first = this->mic_ref_base_.load() + this->mic_read_;
      const uint32_t ref_end = ref_first + FRAME;
      const uint32_t ready = this->ref_ready_.load(std::memory_order_acquire);
      // Wait for a completed reference only while the microphone ring still
      // has enough headroom. When playback stops, pass through raw audio.
      if (this->synchronized_.load(std::memory_order_acquire) && static_cast<int32_t>(ready - ref_end) < 0 &&
          this->mic_written_.load(std::memory_order_acquire) - this->mic_read_ < RING - 2 * FRAME)
        break;
      const bool valid_ref = static_cast<int32_t>(ready - ref_end) >= 0 && ready - ref_first <= RING;
      bool active_ref = false;
      for (size_t i = 0; i < FRAME; ++i) {
        mic_frame[i] = this->mic_ring_[(this->mic_read_ + i) % RING];
        ref_frame[i] = valid_ref ? this->ref_ring_[(ref_first + i) % RING] : 0;
        active_ref |= ref_frame[i] != 0;
      }
      if (this->tx_drops_.load() != last_tx_drops) {
        last_tx_drops = this->tx_drops_.load();
        active_ref = false;
      }
      if (active_ref) {
        this->silent_frames_ = 0;
      } else {
        ++this->silent_frames_;
      }
      auto *aec = this->aec_.load(std::memory_order_acquire);
      const bool processed = aec != nullptr && this->synchronized_.load(std::memory_order_acquire) && valid_ref &&
                             this->silent_frames_ < 5;
#ifdef USE_I2S_AEC_MICROPHONE_DIAGNOSTICS
      if (!audible_tx && !active_ref) {
        if (++probe_idle_frames == ReferenceProbe::WINDOW_FRAMES) {
          ESP_LOGD(TAG, "AEC reference-coherent attenuation=0.0 dB (idle: no playback, not a measurement)");
          probe_idle_frames = 0;
        }
      } else {
        probe_idle_frames = 0;
      }
      // The diagnostic lags are only observations. Never feed a tracked
      // lag back into the AEC's DMA-clock alignment.
      bool probe_collect = processed && this->probe_lag_initialized_;
      const size_t probe_sample_start = (probe_frames * 37U) % ReferenceProbe::SAMPLE_STRIDE;
      std::array<uint16_t, FRAME / ReferenceProbe::SAMPLE_STRIDE> probe_indices{};
      size_t probe_points = 0;
      if (probe_collect) {
        const int32_t phase = this->observed_base_.load(std::memory_order_relaxed) -
                              static_cast<int32_t>(this->mic_ref_base_.load(std::memory_order_relaxed));
        if (probe_frames == 0) {
          probe_phase_min = phase;
          probe_phase_max = phase;
        } else {
          probe_phase_min = std::min(probe_phase_min, phase);
          probe_phase_max = std::max(probe_phase_max, phase);
        }
        // Rotate the sparse sampling phase so periodic music does not always
        // hit the same positions. Keep the original microphone samples before
        // calling the opaque ESP-SR routine, which may modify its input.
        for (size_t i = probe_sample_start; i < FRAME; i += ReferenceProbe::SAMPLE_STRIDE) {
          const int64_t oldest = static_cast<int64_t>(ref_first) + i - probe_lag - ReferenceProbe::MAX_LAG_OFFSET;
          const int64_t newest = static_cast<int64_t>(ref_first) + i - probe_lag + ReferenceProbe::MAX_LAG_OFFSET;
          if (oldest < 0 || newest >= ready || static_cast<int64_t>(ready) - oldest > RING - 2 * FRAME)
            continue;
          probe_indices[probe_points] = i;
          for (int lag = 0; lag < ReferenceProbe::LAG_COUNT; ++lag) {
            const int32_t offset = ReferenceProbe::LAG_OFFSETS[lag];
            const int32_t local_index = static_cast<int32_t>(i) - probe_lag - offset;
            // The AEC's own reference frame is already in fast task-local
            // storage. Read the PSRAM ring only when a lag crosses its edge.
            const int16_t reference =
                local_index >= 0 && local_index < FRAME
                    ? ref_frame[local_index]
                    : this->ref_ring_[static_cast<uint32_t>(static_cast<int64_t>(ref_first) + local_index) % RING];
            probe_reference[probe_points][lag] = reference;
            probe.add_before(lag, reference, mic_frame[i]);
          }
          ++probe_points;
        }
        probe_collect = probe_points != 0;
      }
      if (!probe_collect && probe_frames != 0) {
        reset_probe_window();
      }
#endif
      if (processed) {
        for (auto &sample : ref_frame) {
          const int32_t scaled = static_cast<int32_t>(sample) * this->reference_gain_hundredths_ / 100;
          sample = static_cast<int16_t>(std::clamp<int32_t>(scaled, -32768, 32767));
        }
        aec_process(aec, mic_frame, ref_frame, out_frame);
      } else {
        memcpy(out_frame, mic_frame, sizeof(out_frame));
      }
#ifdef USE_I2S_AEC_MICROPHONE_DIAGNOSTICS
      if (probe_collect) {
        for (size_t probe_point = 0; probe_point < probe_points; ++probe_point) {
          const size_t i = probe_indices[probe_point];
          for (int lag = 0; lag < ReferenceProbe::LAG_COUNT; ++lag)
            probe.add_after(lag, probe_reference[probe_point][lag], out_frame[i]);
        }
      }
      if (probe_collect && ++probe_frames == ReferenceProbe::WINDOW_FRAMES) {
        const uint32_t mic_drops = this->mic_drops_.load(std::memory_order_relaxed);
        const uint32_t tx_drops = this->tx_drops_.load(std::memory_order_relaxed);
        const auto result = probe.result();
        if (result.valid && mic_drops == probe_mic_drops && tx_drops == probe_tx_drops) {
          const int32_t best_lag = probe_lag + ReferenceProbe::LAG_OFFSETS[result.lag_index];
          ESP_LOGD(TAG,
                   "AEC reference-coherent attenuation=%.1f dB, remaining=%.3f, lag=%" PRId32
                   " samples, raw r2=%.2f, ref=%.0f RMS, phase=%" PRId32 "..%" PRId32,
                   20.0f * std::log10(result.remaining), result.remaining, best_lag, result.raw_correlation_squared,
                   result.reference_rms, probe_phase_min, probe_phase_max);
          const int32_t change = best_lag - probe_lag;
          if (change != 0 && change == pending_lag) {
            if (++pending_lag_windows >= 2) {
              probe_lag = best_lag;
              pending_lag_windows = 0;
            }
          } else {
            pending_lag = change;
            pending_lag_windows = change == 0 ? 0 : 1;
          }
        } else {
          ESP_LOGD(TAG,
                   "AEC reference-coherent probe: n/a (no reference/microphone variance or drops=%" PRIu32 "/%" PRIu32
                   ")",
                   mic_drops - probe_mic_drops, tx_drops - probe_tx_drops);
          pending_lag_windows = 0;
        }
        reset_probe_window();
      }
#endif
      if (current_generation != this->generation_.load() || this->listeners_ == 0)
        break;
      memcpy(this->output_.data(), out_frame, sizeof(out_frame));
      this->data_callbacks_.call(this->output_);
      this->mic_read_ += FRAME;
    }
  }
}

}  // namespace esphome::i2s_aec_microphone

#endif
