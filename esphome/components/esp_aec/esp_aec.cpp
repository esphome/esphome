#include "esp_aec.h"

#ifdef USE_ESP32

#include <algorithm>
#include <cstring>

#include "esp_heap_caps.h"
#include "esphome/core/log.h"
#include "../esp_audio_stack/audio_core_scoped_lock.h"

namespace esphome {
namespace esp_aec {

static const char *const TAG = "esp_aec";

// esp-sr HIGH_PERF modes silently calloc the FFT cos/sin tables in
// DMA-capable internal RAM and return a half-init handle on OOM, which
// then crashes process() with LoadProhibited. Pre-flight against the
// largest free block; threshold scales linearly with filter_length
// (40 KB validated for filter_length=4 on S3 codec devices).
static constexpr size_t HIGH_PERF_MIN_DMA_BLOCK_PER_4_TAPS = 40 * 1024;
static constexpr uint32_t DMA_INTERNAL_CAPS = MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT;

static size_t high_perf_min_dma_block_(int filter_length) {
  int multiplier = (filter_length + 3) / 4;
  if (multiplier < 1)
    multiplier = 1;
  return HIGH_PERF_MIN_DMA_BLOCK_PER_4_TAPS * static_cast<size_t>(multiplier);
}

static bool is_high_perf_mode_(aec_mode_t m) {
  const int v = static_cast<int>(m);
  return v == AEC_MODE_SR_HIGH_PERF || v == 4 /* VOIP_HIGH_PERF */ || v == 6 /* FD_HIGH_PERF (esp-sr 2.4+) */;
}

const char *EspAec::get_mode_name(aec_mode_t mode) {
  switch (static_cast<int>(mode)) {
    case AEC_MODE_SR_LOW_COST:
      return "sr_low_cost";
    case AEC_MODE_SR_HIGH_PERF:
      return "sr_high_perf";
    case 3:
      return "voip_low_cost";
    case 4:
      return "voip_high_perf";
    case 5:
      return "fd_low_cost";  // AEC_MODE_FD_LOW_COST  (esp-sr 2.4+)
    case 6:
      return "fd_high_perf";  // AEC_MODE_FD_HIGH_PERF (esp-sr 2.4+)
    default:
      return "sr_low_cost";
  }
}

void EspAec::setup() {
  this->handle_mutex_ = xSemaphoreCreateMutex();
  if (this->handle_mutex_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create handle_mutex_");
    this->mark_failed();
    return;
  }
  // Same pre-flight as reinit_(); fail fast at setup() so a tight build
  // doesn't crash on the first frame.
  if (is_high_perf_mode_(this->mode_)) {
    const size_t threshold = high_perf_min_dma_block_(this->filter_length_);
    const size_t largest = heap_caps_get_largest_free_block(DMA_INTERNAL_CAPS);
    if (largest < threshold) {
      ESP_LOGE(TAG,
               "setup: %s with filter_length=%d needs >=%u bytes contiguous "
               "DMA-capable internal RAM (largest free block: %u). Refusing "
               "to call aec_create which would silently produce a half-init "
               "handle. Use a low_cost mode or reduce internal DMA pressure.",
               get_mode_name(this->mode_), this->filter_length_, (unsigned) threshold, (unsigned) largest);
      vSemaphoreDelete(this->handle_mutex_);
      this->handle_mutex_ = nullptr;
      this->mark_failed();
      return;
    }
  }
  this->handle_ =
      afe_aec_create("MR", this->filter_length_, afe_type_from_mode_(this->mode_), afe_cost_from_mode_(this->mode_));
  if (this->handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create AEC instance");
    vSemaphoreDelete(this->handle_mutex_);
    this->handle_mutex_ = nullptr;
    this->mark_failed();
    return;
  }
  this->cached_frame_size_ = afe_aec_get_chunksize(this->handle_);
  const size_t input_bytes = static_cast<size_t>(this->cached_frame_size_) * 2 * sizeof(int16_t);
  this->input_frame_ =
      static_cast<int16_t *>(heap_caps_aligned_alloc(16, input_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (this->input_frame_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate AEC MR input frame");
    afe_aec_destroy(this->handle_);
    this->handle_ = nullptr;
    vSemaphoreDelete(this->handle_mutex_);
    this->handle_mutex_ = nullptr;
    this->mark_failed();
    return;
  }
  this->last_frame_size_ = this->cached_frame_size_;
}

EspAec::~EspAec() {
  if (this->handle_mutex_ != nullptr) {
    {
      esp_audio_stack::ScopedLock lock(this->handle_mutex_);
      if (this->handle_ != nullptr) {
        afe_aec_destroy(this->handle_);
        this->handle_ = nullptr;
      }
      if (this->input_frame_ != nullptr) {
        heap_caps_free(this->input_frame_);
        this->input_frame_ = nullptr;
      }
    }
    vSemaphoreDelete(this->handle_mutex_);
    this->handle_mutex_ = nullptr;
  } else if (this->handle_ != nullptr) {
    afe_aec_destroy(this->handle_);
    this->handle_ = nullptr;
  }
  if (this->input_frame_ != nullptr) {
    heap_caps_free(this->input_frame_);
    this->input_frame_ = nullptr;
  }
}

void EspAec::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP AEC (ESP-SR standalone):");
  ESP_LOGCONFIG(TAG, "  Sample Rate: %d Hz", this->sample_rate_);
  ESP_LOGCONFIG(TAG, "  Filter Length: %d", this->filter_length_);
  ESP_LOGCONFIG(TAG, "  Frame Size: %d samples", this->cached_frame_size_);
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->get_mode_name());
  ESP_LOGCONFIG(TAG, "  Initialized: %s", this->is_initialized() ? "YES" : "NO");
}

FrameSpec EspAec::frame_spec() const {
  FrameSpec spec;
  spec.sample_rate = this->sample_rate_;
  spec.mic_channels = 1;
  spec.ref_channels = 1;
  spec.input_samples = this->cached_frame_size_;
  spec.output_samples = this->cached_frame_size_;
  return spec;
}

bool EspAec::process(const int16_t *in_mic, const int16_t *in_ref, int16_t *out, uint8_t mic_channels_in) {
  // Single-mic only; mic_channels_in is interface boilerplate. The 10 ms cap
  // serialises against reinit_(); on miss we emit silence, never raw mic audio.
  const size_t frame_bytes = static_cast<size_t>(this->cached_frame_size_) * sizeof(int16_t);
  if (out == nullptr) {
    return false;
  }
  if (in_mic == nullptr) {
    memset(out, 0, frame_bytes);
    return false;
  }
  if (in_ref == nullptr) {
    memset(out, 0, frame_bytes);
    return false;
  }
  esp_audio_stack::ScopedLock lock(this->handle_mutex_, 0);
  if (!lock || this->handle_ == nullptr || this->input_frame_ == nullptr) {
    memset(out, 0, frame_bytes);
    return false;
  }
  const int mic_stride = std::max<int>(1, mic_channels_in);
  for (int i = 0; i < this->cached_frame_size_; i++) {
    this->input_frame_[2 * i] = in_mic[i * mic_stride];
    this->input_frame_[2 * i + 1] = in_ref[i];
  }
  size_t out_bytes = afe_aec_process(this->handle_, this->input_frame_, out);
  if (out_bytes != frame_bytes) {
    return false;
  }
  return true;
}

FeatureControl EspAec::feature_control(AudioFeature feature) const {
  if (feature == AudioFeature::AEC)
    return FeatureControl::RESTART_REQUIRED;
  return FeatureControl::NOT_SUPPORTED;
}

bool EspAec::set_feature(AudioFeature feature, bool enabled) {
  // No per-feature toggles. Use reconfigure() to switch modes.
  return false;
}

ProcessorTelemetry EspAec::telemetry() const {
  ProcessorTelemetry t;
  return t;
}

bool EspAec::reconfigure(int type, int mode) {
  aec_mode_t new_mode;
  // type: 0 SR (wake-word linear), 1 VC (VoIP + residual NL), 2 FD
  //       (full-duplex, esp-sr 2.4+). mode: 0 LOW_COST, 1 HIGH_PERF.
  if (type == 0) {
    new_mode = (mode == 1) ? AEC_MODE_SR_HIGH_PERF : AEC_MODE_SR_LOW_COST;
  } else if (type == 1) {
    new_mode = static_cast<aec_mode_t>((mode == 1) ? 4 : 3);
  } else {
    new_mode = static_cast<aec_mode_t>((mode == 1) ? 6 : 5);
  }
  return this->reinit_(new_mode);
}

bool EspAec::reinit_(aec_mode_t new_mode) {
  if (new_mode == this->mode_) {
    ESP_LOGD(TAG, "AEC mode already %s; skipping reinit", get_mode_name(new_mode));
    return true;
  }

  ESP_LOGI(TAG, "Reinitializing AEC: mode %d -> %d (%s -> %s)", (int) this->mode_, (int) new_mode,
           get_mode_name(this->mode_), get_mode_name(new_mode));
  if (this->handle_mutex_ == nullptr) {
    ESP_LOGE(TAG, "reinit_: handle_mutex_ not initialized");
    return false;
  }

  // Pre-flight before the handle mutex so a rejected switch leaves the
  // audio task on the previous working mode, no frame_spec bump, no gap.
  if (is_high_perf_mode_(new_mode)) {
    const size_t threshold = high_perf_min_dma_block_(this->filter_length_);
    size_t largest_internal = heap_caps_get_largest_free_block(DMA_INTERNAL_CAPS);
    if (largest_internal < threshold) {
      ESP_LOGE(TAG,
               "reinit_: %s with filter_length=%d needs >=%u bytes contiguous "
               "DMA-capable internal RAM (largest free block: %u). Rejecting "
               "mode change to avoid silent fft calloc fail in aec_create. "
               "Keeping mode=%s. Reduce internal DMA pressure to use this mode.",
               get_mode_name(new_mode), this->filter_length_, (unsigned) threshold, (unsigned) largest_internal,
               get_mode_name(this->mode_));
      return false;
    }
  }

  esp_audio_stack::ScopedLock lock(this->handle_mutex_);
  afe_aec_handle_t *old_handle = this->handle_;
  int16_t *old_input_frame = this->input_frame_;
  aec_mode_t old_mode = this->mode_;
  // Build the next handle first; roll back if create fails.
  afe_aec_handle_t *new_handle =
      afe_aec_create("MR", this->filter_length_, afe_type_from_mode_(new_mode), afe_cost_from_mode_(new_mode));
  if (new_handle == nullptr) {
    ESP_LOGE(TAG, "Failed to create new AEC instance, keeping previous mode=%s", get_mode_name(old_mode));
    return false;
  }
  int new_frame_size = afe_aec_get_chunksize(new_handle);
  const size_t input_bytes = static_cast<size_t>(new_frame_size) * 2 * sizeof(int16_t);
  int16_t *new_input_frame =
      static_cast<int16_t *>(heap_caps_aligned_alloc(16, input_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (new_input_frame == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate new AEC MR input frame, keeping previous mode=%s", get_mode_name(old_mode));
    afe_aec_destroy(new_handle);
    return false;
  }
  this->handle_ = new_handle;
  this->input_frame_ = new_input_frame;
  this->mode_ = new_mode;
  this->cached_frame_size_ = new_frame_size;
  if (old_handle != nullptr) {
    afe_aec_destroy(old_handle);
  }
  if (old_input_frame != nullptr) {
    heap_caps_free(old_input_frame);
  }
  if (this->cached_frame_size_ != this->last_frame_size_) {
    this->last_frame_size_ = this->cached_frame_size_;
    this->frame_spec_revision_.fetch_add(1, std::memory_order_release);
  }
  ESP_LOGI(TAG, "AEC reinitialized: mode=%d, frame=%d (%dms)", (int) this->mode_, this->cached_frame_size_,
           this->cached_frame_size_ * 1000 / this->sample_rate_);
  return true;
}

afe_type_t EspAec::afe_type_from_mode_(aec_mode_t mode) {
  switch (static_cast<int>(mode)) {
    case AEC_MODE_SR_LOW_COST:
    case AEC_MODE_SR_HIGH_PERF:
      return AFE_TYPE_SR;
    case 5:
    case 6:
      return static_cast<afe_type_t>(3);  // AFE_TYPE_FD in esp-sr 2.4+
    default:
      return AFE_TYPE_VC;
  }
}

afe_mode_t EspAec::afe_cost_from_mode_(aec_mode_t mode) {
  return is_high_perf_mode_(mode) ? AFE_MODE_HIGH_PERF : AFE_MODE_LOW_COST;
}

}  // namespace esp_aec
}  // namespace esphome

#endif  // USE_ESP32
