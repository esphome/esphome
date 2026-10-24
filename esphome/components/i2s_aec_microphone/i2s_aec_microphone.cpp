#include "i2s_aec_microphone.h"

#if defined(USE_ESP32) && defined(USE_ESP_IDF) && defined(USE_I2S_AEC_MICROPHONE)

#include "esphome/core/log.h"

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
      (this->reference_sample_rate_ != RATE && this->reference_sample_rate_ != 3 * RATE)) {
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
  if (this->reference_sample_rate_ == 3 * RATE) {
    auto resampler = std::make_unique<esp_audio_libs::resampler::Resampler>(480, 512);
    esp_audio_libs::resampler::ResamplerConfiguration config = {
        .source_sample_rate = 48000.0f,
        .target_sample_rate = 16000.0f,
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
      this->reference_output_.resize(512 * sizeof(int16_t));
    }
  }
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

void I2SAECMicrophone::dump_config() {
  ESP_LOGCONFIG(TAG, "Live ESP-SR AEC: %" PRIu32 " -> 16 kHz, filter=%d, core=%d, ready=%s",
                this->reference_sample_rate_, this->filter_length_, this->task_core_, YESNO(!this->is_failed()));
}

void I2SAECMicrophone::start() {
  if (this->is_failed() || this->listeners_ == UINT8_MAX)
    return;
  if (this->listeners_.fetch_add(1) == 0) {
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
  if (bytes == 0 || bytes > 480 * frame_bytes || bytes % (ratio * frame_bytes) != 0) {
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
    if (!this->tx_was_active_) {
      this->observed_base_filter_ = observed;
    } else {
      this->observed_base_filter_ += (observed - this->observed_base_filter_) / 32;
    }
    this->observed_base_.store(this->observed_base_filter_, std::memory_order_release);
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
    }
  }
  if (best_frame_scores[0] < minimum_correlation_squared || best_frame_scores[1] < minimum_correlation_squared)
    return false;

  // Both this origin and the phase monitor come from the same callback-sampled
  // clock difference. Sampling the counters here in the AEC task instead would
  // add variable worker and resampler scheduling delay.
  this->mic_ref_base_.store(static_cast<uint32_t>(this->observed_base_.load(std::memory_order_acquire)),
                            std::memory_order_relaxed);
  this->synchronized_.store(true, std::memory_order_release);
  ESP_LOGI(TAG, "AEC reference synchronized");
  return true;
}

void I2SAECMicrophone::task_entry(void *context) { static_cast<I2SAECMicrophone *>(context)->run_(); }

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
    size_t count = block.count;
    if (this->reference_sample_rate_ == 3 * RATE) {
      const auto result = this->reference_resampler_->resample(samples, this->reference_output_.data(), block.count,
                                                               this->reference_output_.size() / sizeof(int16_t), 0.0f);
      count = result.frames_used == block.count ? result.frames_generated : 0;
      samples = this->reference_output_.data();
    }
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
      if (std::abs(observed - current) > static_cast<int32_t>(FRAME / 2)) {
        this->synchronized_.store(false, std::memory_order_release);
        ESP_LOGW(TAG,
                 "AEC reference phase changed by %" PRId32 " samples (observed=%" PRId32 ", base=%" PRId32
                 ", TX queued=%" PRIu32 ", RX=%" PRIu32 "); recalibrating",
                 observed - current, observed, current,
                 this->tx_total_.load(std::memory_order_relaxed) - this->ref_ready_.load(std::memory_order_relaxed),
                 mic_write_for_calibration);
      }
    }
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
      if (processed) {
        for (auto &sample : ref_frame) {
          const int32_t scaled = static_cast<int32_t>(sample) * this->reference_gain_hundredths_ / 100;
          sample = static_cast<int16_t>(std::clamp<int32_t>(scaled, -32768, 32767));
        }
        aec_process(aec, mic_frame, ref_frame, out_frame);
      } else {
        memcpy(out_frame, mic_frame, sizeof(out_frame));
      }
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
