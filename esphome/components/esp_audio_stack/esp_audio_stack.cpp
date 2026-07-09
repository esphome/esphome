#include "esp_audio_stack.h"

#ifdef USE_ESP32

#include <driver/i2s_common.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_timer.h>
#include <gain.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "audio_core_processor.h"
#include "audio_core_ring_buffer_caps.h"
#include "audio_core_scoped_lock.h"
#include "audio_core_task_utils.h"

namespace esphome::esp_audio_stack {

static const char *const TAG = "audio_stack";

// Audio parameters
// IDF default I2S channel config uses 6 descriptors. Keep the old ESPHome
// duplex latency policy while using the official esp_driver_i2s API directly:
// one descriptor carries ~10 ms of audio, clamped to the 4092-byte DMA limit.
constexpr size_t DMA_BUFFER_COUNT = 6;
constexpr uint32_t DMA_BUFFER_DURATION_MS = 10;
// Minimum speaker buffer for very small configured durations. The default comes
// from the speaker platform schema (`buffer_duration: 500ms`) to match native
// ESPHome I2S speaker semantics.
constexpr size_t SPEAKER_BUFFER_MIN_BYTES = 2048;

namespace {
constexpr float SOFTWARE_VOLUME_MIN_DB = -49.0f;

float sanitize_volume_min_db(float db) {
  if (!std::isfinite(db))
    return SOFTWARE_VOLUME_MIN_DB;
  if (db < -96.0f)
    return -96.0f;
  if (db > 0.0f)
    return 0.0f;
  return db;
}

int32_t volume_factor_to_q31(float volume, float min_db = SOFTWARE_VOLUME_MIN_DB) {
  if (!(volume > 0.0f))
    return 0;
  if (volume >= 1.0f)
    return INT32_MAX;
  return esp_audio_libs::gain::db_to_q31(remap<float, float>(volume, 0.0f, 1.0f, sanitize_volume_min_db(min_db), 0.0f));
}

int32_t attenuation_factor_to_q31(float gain) {
  if (!(gain > 0.0f))
    return 0;
  if (gain >= 1.0f)
    return INT32_MAX;
  return esp_audio_libs::gain::db_to_q31(20.0f * std::log10(gain));
}

int8_t gain_factor_to_positive_db(float gain) {
  if (!(gain > 1.0f))
    return 0;
  const int32_t db = static_cast<int32_t>(std::lround(20.0f * std::log10(gain)));
  if (db <= 0)
    return 0;
  if (db > 30)
    return 30;
  return static_cast<int8_t>(db);
}

[[maybe_unused]] int32_t multiply_q31(int32_t a, int32_t b) {
  if (a <= 0 || b <= 0)
    return 0;
  const int64_t value = (static_cast<int64_t>(a) * static_cast<int64_t>(b) + (1LL << 30)) >> 31;
  return value >= INT32_MAX ? INT32_MAX : static_cast<int32_t>(value);
}

uint32_t largest_divisor_at_most(uint32_t value, uint32_t limit, uint32_t minimum) {
  if (value == 0 || limit == 0)
    return 0;
  uint32_t candidate = std::min(value, limit);
  while (candidate > minimum) {
    if (value % candidate == 0)
      return candidate;
    candidate--;
  }
  return value % minimum == 0 && minimum <= limit ? minimum : std::min(value, limit);
}

float sanitize_gain_factor(float gain) {
  if (!std::isfinite(gain) || gain <= 0.0f) {
    return 0.0f;
  }
  // YAML exposes input gain up to 32x and mic_gain_db +30 dB maps to
  // ~31.6x. Clamp direct C++ callers to the same practical envelope.
  return gain > 32.0f ? 32.0f : gain;
}
}  // namespace

void ESPAudioStack::set_mic_gain(float gain) {
  gain = sanitize_gain_factor(gain);
  this->mic_gain_.store(gain, std::memory_order_relaxed);
  this->mic_gain_q31_.store(attenuation_factor_to_q31(gain), std::memory_order_relaxed);
  this->mic_gain_boost_db_.store(gain_factor_to_positive_db(gain), std::memory_order_relaxed);
}

void ESPAudioStack::set_input_gain(float gain) {
  gain = sanitize_gain_factor(gain);
  this->input_gain_.store(gain, std::memory_order_relaxed);
  this->input_gain_q31_.store(attenuation_factor_to_q31(gain), std::memory_order_relaxed);
  this->input_gain_boost_.store(gain > 1.0f ? gain : 1.0f, std::memory_order_relaxed);
}

void ESPAudioStack::set_master_volume_min_db(float db) {
  db = sanitize_volume_min_db(db);
  const float previous = this->master_volume_min_db_;
  this->master_volume_min_db_ = db;
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  this->codec_backend_.set_output_volume_curve(db);
#endif
  if (previous != db)
    this->set_master_volume(this->master_volume_linear_.load(std::memory_order_relaxed));
}

void ESPAudioStack::set_master_volume(float volume) {
  if (!(volume > 0.0f)) {
    volume = 0.0f;
  } else if (volume > 1.0f) {
    volume = 1.0f;
  }
  this->master_volume_linear_.store(volume, std::memory_order_relaxed);
  const int32_t q31 = volume_factor_to_q31(volume, this->master_volume_min_db_);
  const int32_t previous = this->master_volume_q31_.exchange(q31, std::memory_order_relaxed);
  if (previous != q31)
    this->update_hot_output_volume_();
}

void ESPAudioStack::set_master_volume_q31(int32_t q31) {
  if (q31 < 0)
    q31 = 0;
  this->master_volume_linear_.store(static_cast<float>(q31) / static_cast<float>(INT32_MAX), std::memory_order_relaxed);
  const int32_t previous = this->master_volume_q31_.exchange(q31, std::memory_order_relaxed);
  if (previous != q31)
    this->update_hot_output_volume_();
}

void ESPAudioStack::set_output_volume(float volume) {
  if (!(volume > 0.0f)) {
    volume = 0.0f;
  } else if (volume > 1.0f) {
    volume = 1.0f;
  }
  this->set_output_volume_q31(volume_factor_to_q31(volume));
}

void ESPAudioStack::set_output_volume_q31(int32_t q31) {
  if (q31 < 0)
    q31 = 0;
  const int32_t previous = this->output_volume_q31_.exchange(q31, std::memory_order_relaxed);
  if (previous != q31)
    this->update_hot_output_volume_();
}

void ESPAudioStack::update_hot_output_volume_() {
  const int32_t output_q31 = this->output_volume_q31_.load(std::memory_order_relaxed);
  const int32_t master_q31 = this->master_volume_q31_.load(std::memory_order_relaxed);
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_OUTPUT_CODEC
  const bool hardware_master = true;
  const float public_master = this->master_volume_linear_.load(std::memory_order_relaxed);
  const int32_t combined_q31 = output_q31;
#else
  const bool hardware_master = false;
  const float public_master = static_cast<float>(master_q31) / static_cast<float>(INT32_MAX);
  const int32_t combined_q31 = multiply_q31(output_q31, master_q31);
#endif
  const float linear = static_cast<float>(combined_q31) / static_cast<float>(INT32_MAX);
  this->master_volume_public_.store(public_master, std::memory_order_relaxed);
  const int32_t previous = this->hot_output_volume_q31_.exchange(combined_q31, std::memory_order_relaxed);
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  if (hardware_master) {
    this->codec_backend_.set_output_volume(public_master);
  }
#endif
  if (previous != combined_q31) {
    ESP_LOGD(TAG, "Master volume: output_q31=%d master_q31=%d hot_q31=%d linear=%.3f hw_master=%s previous_q31=%d",
             output_q31, master_q31, combined_q31, linear, hardware_master ? "yes" : "no", previous);
  }
}

const char *ESPAudioStack::runtime_state_to_string(AudioStackRuntimeState state) {
  switch (state) {
    case AudioStackRuntimeState::IDLE:
      return "idle";
    case AudioStackRuntimeState::MIC:
      return "mic";
    case AudioStackRuntimeState::SPEAKER:
      return "speaker";
    case AudioStackRuntimeState::DUPLEX:
      return "duplex";
  }
  return "unknown";
}

AudioStackRuntimeState ESPAudioStack::compute_runtime_state_() const {
  const bool mic = this->has_mic_consumers_.load(std::memory_order_relaxed);
  const bool speaker = this->speaker_running_.load(std::memory_order_relaxed);
  if (mic && speaker)
    return AudioStackRuntimeState::DUPLEX;
  if (mic)
    return AudioStackRuntimeState::MIC;
  if (speaker)
    return AudioStackRuntimeState::SPEAKER;
  return AudioStackRuntimeState::IDLE;
}

void ESPAudioStack::update_runtime_state_() {
  const auto next = this->compute_runtime_state_();
  const auto next_raw = static_cast<uint8_t>(next);
  const auto prev_raw = this->runtime_state_.exchange(next_raw, std::memory_order_relaxed);
  if (prev_raw == next_raw)
    return;
  const char *state = runtime_state_to_string(next);
  ESP_LOGD(TAG, "Runtime state: %s", state);
  this->state_trigger_.trigger(std::string(state));
}

const char *ESPAudioStack::i2s_hardware_state_to_string(I2SHardwareState state) {
  switch (state) {
    case I2SHardwareState::UNPREPARED:
      return "unprepared";
    case I2SHardwareState::PREPARING:
      return "preparing";
    case I2SHardwareState::READY:
      return "ready";
    case I2SHardwareState::RUNNING:
      return "running";
    case I2SHardwareState::STOPPING:
      return "stopping";
    case I2SHardwareState::ERROR:
      return "error";
  }
  return "unknown";
}

void ESPAudioStack::set_i2s_hardware_state_(I2SHardwareState state) {
  const auto next_raw = static_cast<uint8_t>(state);
  const auto prev_raw = this->i2s_hardware_state_.exchange(next_raw, std::memory_order_relaxed);
  if (prev_raw != next_raw) {
    ESP_LOGD(TAG, "I2S hardware state: %s", i2s_hardware_state_to_string(state));
  }
}

void ESPAudioStack::service_speaker_reset_() {
  if (!this->request_speaker_reset_.exchange(false, std::memory_order_relaxed)) {
    return;
  }
  if (this->speaker_buffer_) {
    this->speaker_buffer_->reset();
  }
#ifdef USE_ESP_AUDIO_STACK_MONO_REF
  this->direct_aec_ref_valid_ = false;
#endif
#ifdef USE_ESP_AUDIO_STACK_RING_REF
  if (this->aec_ref_ring_buffer_) {
    this->aec_ref_ring_buffer_->reset();
  }
#endif
}

void ESPAudioStack::log_memory_snapshot_(const char *label) const {
  ESP_LOGI(TAG, "Memory[%s]: internal_free=%u largest_internal=%u dma_free=%u largest_dma=%u psram_free=%u", label,
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_DMA),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

// Helper: get MCLK multiple enum from integer value
static i2s_mclk_multiple_t get_mclk_multiple(uint32_t mult) {
  switch (mult) {
    case 128:
      return I2S_MCLK_MULTIPLE_128;
    case 384:
      return I2S_MCLK_MULTIPLE_384;
    case 512:
      return I2S_MCLK_MULTIPLE_512;
    default:
      return I2S_MCLK_MULTIPLE_256;
  }
}

// Helper: get STD slot config for the configured comm format (0=philips, 1=msb, 2=pcm_short, 3=pcm_long)
// Note: PCM short/long are TDM-only in ESP-IDF; falls back to Philips in STD mode
static i2s_std_slot_config_t get_std_slot_config(uint8_t fmt, i2s_data_bit_width_t bw, i2s_slot_mode_t mode) {
  switch (fmt) {
    case 1:
      return I2S_STD_MSB_SLOT_DEFAULT_CONFIG(bw, mode);
    default:
      return I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bw, mode);
  }
}

#if SOC_I2S_SUPPORTS_TDM && defined(USE_ESP_AUDIO_STACK_TDM_BUS)
// Helper: get TDM slot config for the configured comm format
static i2s_tdm_slot_config_t get_tdm_slot_config(uint8_t fmt, i2s_data_bit_width_t bw, i2s_slot_mode_t mode,
                                                 i2s_tdm_slot_mask_t mask) {
  switch (fmt) {
    case 1:
      return I2S_TDM_MSB_SLOT_DEFAULT_CONFIG(bw, mode, mask);
    case 2:
      return I2S_TDM_PCM_SHORT_SLOT_DEFAULT_CONFIG(bw, mode, mask);
    case 3:
      return I2S_TDM_PCM_LONG_SLOT_DEFAULT_CONFIG(bw, mode, mask);
    default:
      return I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(bw, mode, mask);
  }
}
#endif  // SOC_I2S_SUPPORTS_TDM

void ESPAudioStack::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP Audio Stack (ESP-IDF I2S + esp_codec_dev backend)...");

  // Mutex for the mic consumer registry. Plain FreeRTOS mutex (used as lock,
  // not as a counting semaphore); replaces std::mutex to keep the public
  // header free of <mutex>.
  this->mic_consumers_mutex_ = xSemaphoreCreateMutex();
  if (this->mic_consumers_mutex_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create mic_consumers_mutex_");
    this->mark_failed();
    return;
  }

  // Compute rate-conversion ratio. The Espressif audio-effects wrappers are also
  // used at ratio 1 for 32-bit -> 16-bit conversion, so initialise them
  // unconditionally.
  if (this->output_sample_rate_ > 0 && this->output_sample_rate_ != this->sample_rate_) {
    this->rate_conversion_ratio_ = this->sample_rate_ / this->output_sample_rate_;
    if (this->rate_conversion_ratio_ * this->output_sample_rate_ != this->sample_rate_) {
      ESP_LOGE(TAG, "sample_rate (%u) must be an exact multiple of output_sample_rate (%u)",
               (unsigned) this->sample_rate_, (unsigned) this->output_sample_rate_);
      this->mark_failed();
      return;
    }
    if (this->rate_conversion_ratio_ > 6) {
      ESP_LOGE(TAG, "Rate conversion ratio %u exceeds maximum of 6", (unsigned) this->rate_conversion_ratio_);
      this->mark_failed();
      return;
    }
    ESP_LOGI(TAG, "Multi-rate: bus=%uHz, output=%uHz, ratio=%u", (unsigned) this->sample_rate_,
             (unsigned) this->output_sample_rate_, (unsigned) this->rate_conversion_ratio_);
  }
#ifdef USE_ESP_AUDIO_STACK_MONO_RX
  this->mic_rate_converter_.init(this->rate_conversion_ratio_, this->sample_rate_, this->get_output_sample_rate(),
                                 this->rate_cvt_complexity_, this->rate_cvt_perf_type_);
#endif
#ifdef USE_ESP_AUDIO_STACK_MONO_REF
  this->play_ref_rate_converter_.init(this->rate_conversion_ratio_, this->sample_rate_, this->get_output_sample_rate(),
                                      this->rate_cvt_complexity_, this->rate_cvt_perf_type_);
#endif
  // rx_rate_converter_ is initialized inside audio_session_ once the processor has
  // reported its frame_spec and we know how many channels the RX stream carries.

  // Speaker ring buffer: stores the public ESPHome speaker stream at bus rate.
  // Most full-duplex profiles keep this mono even when the physical TX bus is
  // stereo for codec feedback/reference. Opt-in stereo speakers store
  // interleaved L/R.
  // PREFER_PSRAM: staging buffer between API play() and the i2s write path, not
  // realtime-critical itself (the task drains it at priority 19), so PSRAM is fine.
  const size_t speaker_bytes_per_second = this->sample_rate_ * sizeof(int16_t) * this->get_speaker_channels();
  this->speaker_buffer_size_ =
      std::max<size_t>(SPEAKER_BUFFER_MIN_BYTES,
                       (speaker_bytes_per_second * static_cast<size_t>(this->speaker_buffer_duration_ms_)) / 1000);
  this->speaker_buffer_ = create_prefer_psram(this->speaker_buffer_size_, "audio_stack.speaker");
  if (!this->speaker_buffer_) {
    ESP_LOGE(TAG, "Failed to create speaker ring buffer (%u bytes)", (unsigned) this->speaker_buffer_size_);
    this->mark_failed();
    return;
  }
  this->log_memory_snapshot_("after_speaker_ring");

  // AEC reference (mono mode only; stereo/TDM get ref from I2S RX).
  // direct_aec_ref_ is allocated by allocate_audio_buffers_() once the
  // processor frame spec is known. Storage matches input_frame_bytes because
  // AudioProcessor::process consumes in_ref at input_samples length; the TX-side
  // rate converter writes one input-side reference frame here at the processor rate,
  // not at the bus rate.

  // Create the permanent audio task during component setup, then park it
  // until start() flips audio_stack_running_. This reserves the stack/TCB before
  // Wi-Fi/API/VA/MWW churn can fragment internal RAM, and removes xTaskCreate
  // from the first wake-word/audio activation path.
  const BaseType_t core = this->task_core_ >= 0 ? this->task_core_ : tskNO_AFFINITY;
  if (!start_pinned_task(audio_task, "audio_stack", this->task_stack_size_, this, this->task_priority_, core,
                         this->audio_task_stack_in_psram_, TAG, &this->audio_task_handle_, &this->audio_task_tcb_,
                         &this->audio_task_stack_)) {
    ESP_LOGE(TAG, "Failed to create permanent audio task");
    this->has_i2s_error_.store(true, std::memory_order_relaxed);
    this->mark_failed();
    return;
  }

  // Reserve the hot-path RX/TX/processor buffers as early as the real frame
  // shape allows. The allocation itself runs on the parked audio task, so this
  // does not move heavy heap work into the ESPHome setup thread. If processor
  // setup has not published a frame_spec yet, loop() will retry.
  this->request_audio_preallocation_();

  ESP_LOGI(TAG, "ESP Audio Stack ready (speaker_buf=%u bytes, task precreated)", (unsigned) this->speaker_buffer_size_);
}

void ESPAudioStack::set_processor(AudioProcessor *processor) {
  this->processor_ = processor;
  this->processor_enabled_.store(processor != nullptr, std::memory_order_relaxed);
  // Note: direct_aec_ref_ is allocated later in allocate_audio_buffers_() once
  // the processor frame spec is known for the current audio session.
}

void ESPAudioStack::sync_processor_background_consumer_() {
#ifdef USE_AUDIO_PROCESSOR
  const bool want_background = this->processor_ != nullptr && this->processor_->wants_background_input();
  const bool registered = this->processor_background_consumer_registered_.load(std::memory_order_relaxed);

  if (want_background && !registered) {
    if (this->register_mic_consumer(this)) {
      this->processor_background_consumer_registered_.store(true, std::memory_order_relaxed);
      ESP_LOGI(TAG, "Processor background mic consumer registered");
    }
  } else if (!want_background && registered) {
    this->processor_background_consumer_registered_.store(false, std::memory_order_relaxed);
    this->unregister_mic_consumer(this);
    ESP_LOGI(TAG, "Processor background mic consumer unregistered");
  }
#endif
}

void ESPAudioStack::request_audio_preallocation_() {
  if (this->prealloc_attempted_.load(std::memory_order_acquire) ||
      this->prealloc_requested_.load(std::memory_order_acquire) || this->audio_task_handle_ == nullptr) {
    return;
  }
  bool frame_shape_known = true;
#ifdef USE_AUDIO_PROCESSOR
  if (this->processor_ != nullptr) {
    auto spec = this->processor_->frame_spec();
    frame_shape_known = spec.input_samples > 0 && spec.output_samples > 0;
  }
#endif
  if (!frame_shape_known) {
    return;
  }
  this->prealloc_requested_.store(true, std::memory_order_release);
  xTaskNotifyGive(this->audio_task_handle_);
}

void ESPAudioStack::loop() {
  this->sync_processor_background_consumer_();

  if (!this->audio_stack_running_.load(std::memory_order_relaxed) &&
      this->audio_task_idle_.load(std::memory_order_relaxed)) {
    this->request_audio_preallocation_();
  }

  // Pick up the deferred I2S release queued by stop(). The task may still be
  // blocked inside a codec/I2S transfer for the current frame, so delete
  // channels only after it has parked in the outer wait loop.
  if (this->teardown_pending_.load(std::memory_order_relaxed) &&
      this->audio_task_idle_.load(std::memory_order_relaxed)) {
    this->deinit_i2s_();
    this->teardown_pending_.store(false, std::memory_order_relaxed);
    ESP_LOGI(TAG, "Audio stack stopped");
  }
}

void ESPAudioStack::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP Audio Stack:");
#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  if (this->dual_i2s_bus_) {
    ESP_LOGCONFIG(TAG, "  I2S Layout: dual bus");
    ESP_LOGCONFIG(TAG, "  RX Bus: port=%u LRCLK=%d BCLK=%d MCLK=%d DIN=%d", this->rx_bus_.i2s_num,
                  this->rx_bus_.lrclk_pin, this->rx_bus_.bclk_pin, this->rx_bus_.mclk_pin, this->rx_bus_.din_pin);
    ESP_LOGCONFIG(TAG, "  TX Bus: port=%u LRCLK=%d BCLK=%d MCLK=%d DOUT=%d", this->tx_bus_.i2s_num,
                  this->tx_bus_.lrclk_pin, this->tx_bus_.bclk_pin, this->tx_bus_.mclk_pin, this->tx_bus_.dout_pin);
  } else {
    ESP_LOGCONFIG(TAG, "  I2S Layout: shared bus");
    ESP_LOGCONFIG(TAG, "  LRCLK Pin: %d", this->lrclk_pin_);
    ESP_LOGCONFIG(TAG, "  BCLK Pin: %d", this->bclk_pin_);
    ESP_LOGCONFIG(TAG, "  MCLK Pin: %d", this->mclk_pin_);
    ESP_LOGCONFIG(TAG, "  DIN Pin: %d", this->din_pin_);
    ESP_LOGCONFIG(TAG, "  DOUT Pin: %d", this->dout_pin_);
    ESP_LOGCONFIG(TAG, "  I2S Port: %u", this->i2s_num_);
  }
#else
  ESP_LOGCONFIG(TAG, "  I2S Layout: shared bus");
  ESP_LOGCONFIG(TAG, "  LRCLK Pin: %d", this->lrclk_pin_);
  ESP_LOGCONFIG(TAG, "  BCLK Pin: %d", this->bclk_pin_);
  ESP_LOGCONFIG(TAG, "  MCLK Pin: %d", this->mclk_pin_);
  ESP_LOGCONFIG(TAG, "  DIN Pin: %d", this->din_pin_);
  ESP_LOGCONFIG(TAG, "  DOUT Pin: %d", this->dout_pin_);
  ESP_LOGCONFIG(TAG, "  I2S Port: %u", this->i2s_num_);
#endif
  ESP_LOGCONFIG(TAG, "  I2S Role: %s", this->i2s_mode_secondary_ ? "secondary (peripheral)" : "primary (master)");
  ESP_LOGCONFIG(TAG, "  I2S Bus Rate: %u Hz", (unsigned) this->sample_rate_);
  ESP_LOGCONFIG(TAG, "  I2S Bits Per Sample: %u", this->bits_per_sample_);
  if (this->slot_bit_width_ > 0) {
    ESP_LOGCONFIG(TAG, "  Slot Bit Width: %u", this->slot_bit_width_);
  }
  ESP_LOGCONFIG(TAG, "  TX Channels: %u (%s)", this->num_channels_, this->num_channels_ == 2 ? "stereo" : "mono");
  ESP_LOGCONFIG(TAG, "  Speaker Stream Channels: %u", this->get_speaker_channels());
  ESP_LOGCONFIG(TAG, "  RX Mic Channel: %s", this->mic_channel_right_ ? "RIGHT" : "LEFT");
  ESP_LOGCONFIG(TAG, "  RX Slot Mode: %s", this->rx_slot_mode_stereo_ ? "stereo" : "mono");
  static const char *const FMT_NAMES[] = {"Philips", "MSB", "PCM Short", "PCM Long"};
  ESP_LOGCONFIG(TAG, "  Comm Format: %s", FMT_NAMES[this->i2s_comm_fmt_ & 3]);
  ESP_LOGCONFIG(TAG, "  MCLK Multiple: %u", (unsigned) this->mclk_multiple_);
  if (this->use_apll_) {
    ESP_LOGCONFIG(TAG, "  APLL: enabled");
  }
  if (this->correct_dc_offset_) {
    ESP_LOGCONFIG(TAG, "  DC Offset Correction: enabled");
  }
  if (this->rate_conversion_ratio_ > 1) {
    ESP_LOGCONFIG(TAG, "  Output Rate: %u Hz (rate conversion x%u)", (unsigned) this->get_output_sample_rate(),
                  (unsigned) this->rate_conversion_ratio_);
    ESP_LOGCONFIG(TAG, "  Rate Converter: esp_ae_rate_cvt");
    ESP_LOGCONFIG(TAG, "  Rate Converter Complexity: %u", (unsigned) this->rate_cvt_complexity_);
    ESP_LOGCONFIG(TAG, "  Rate Converter Perf: %s", this->rate_cvt_perf_type_ == 0 ? "memory" : "speed");
  }
  ESP_LOGCONFIG(TAG, "  Speaker Buffer: %u bytes (%u ms)", (unsigned) this->speaker_buffer_size_,
                (unsigned) this->speaker_buffer_duration_ms_);
  if (this->use_stereo_aec_ref_) {
    ESP_LOGCONFIG(TAG, "  Stereo AEC Reference: %s channel", this->ref_channel_right_ ? "RIGHT" : "LEFT");
  }
  if (this->use_tdm_bus_) {
    if (this->tdm_second_mic_slot_ >= 0) {
      ESP_LOGCONFIG(TAG, "  TDM Reference: %u slots, mic_slots=[%u,%d], ref_slot=%u, tx_slot=%u",
                    this->tdm_total_slots_, this->tdm_mic_slot_, this->tdm_second_mic_slot_, this->tdm_ref_slot_,
                    this->tdm_tx_slot_);
    } else {
      ESP_LOGCONFIG(TAG, "  TDM Reference: %u slots, mic_slot=%u, ref_slot=%u, tx_slot=%u", this->tdm_total_slots_,
                    this->tdm_mic_slot_, this->tdm_ref_slot_, this->tdm_tx_slot_);
    }
  }
  ESP_LOGCONFIG(TAG, "  AEC: %s", this->processor_ != nullptr ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Task: priority=%u, core=%d, stack=%u", this->task_priority_, this->task_core_,
                (unsigned) this->task_stack_size_);
  ESP_LOGCONFIG(TAG, "  I2S Lifecycle: esp_driver_i2s create on start, delete on idle stop");
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  ESP_LOGCONFIG(TAG, "  Codec Backend: esp_codec_dev direct read/write (input=%s, output=%s)",
                this->codec_backend_.input_codec_name(), this->codec_backend_.output_codec_name());
#else
  ESP_LOGCONFIG(TAG, "  IO Backend: esp_driver_i2s direct read/write (no hardware codec)");
#endif
  ESP_LOGCONFIG(TAG, "  I2S Hardware State: %s",
                i2s_hardware_state_to_string(
                    static_cast<I2SHardwareState>(this->i2s_hardware_state_.load(std::memory_order_relaxed))));
#ifdef USE_ESP_AUDIO_STACK_TELEMETRY
  ESP_LOGCONFIG(TAG, "  Telemetry Log Interval: %u frames", (unsigned) this->telemetry_log_interval_frames_);
#endif
}

bool ESPAudioStack::prepare_i2s_channels_() {
  if (this->tx_handle_ != nullptr || this->rx_handle_ != nullptr) {
    return true;
  }

  ESP_LOGCONFIG(TAG, "Preparing I2S channels in full-duplex mode...");
  this->set_i2s_hardware_state_(I2SHardwareState::PREPARING);
  this->log_memory_snapshot_("before_i2s_prepare");

  // Map configured bit depth to I2S enum
  // Note: 24-bit data is stored in 32-bit DMA containers (MSB-aligned)
  i2s_data_bit_width_t bit_width;
  switch (this->bits_per_sample_) {
    case 32:
      bit_width = I2S_DATA_BIT_WIDTH_32BIT;
      break;
    case 24:
      bit_width = I2S_DATA_BIT_WIDTH_24BIT;
      break;
    default:
      bit_width = I2S_DATA_BIT_WIDTH_16BIT;
      break;
  }

  // Slot bit width: auto = match data bit width, or explicit override
  i2s_slot_bit_width_t slot_bw = I2S_SLOT_BIT_WIDTH_AUTO;
  if (this->slot_bit_width_ > 0) {
    switch (this->slot_bit_width_) {
      case 32:
        slot_bw = I2S_SLOT_BIT_WIDTH_32BIT;
        break;
      case 24:
        slot_bw = I2S_SLOT_BIT_WIDTH_24BIT;
        break;
      case 16:
        slot_bw = I2S_SLOT_BIT_WIDTH_16BIT;
        break;
      default:
        slot_bw = I2S_SLOT_BIT_WIDTH_AUTO;
        break;
    }
  }

#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  const bool dual_bus = this->dual_i2s_bus_;
#else
  constexpr bool dual_bus = false;
#endif
  if (dual_bus && this->use_tdm_bus_) {
    ESP_LOGE(TAG, "dual I2S bus currently supports standard I2S only; TDM reference requires shared bus");
    this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
    return false;
  }

#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  const uint8_t tx_i2s_num = dual_bus ? this->tx_bus_.i2s_num : this->i2s_num_;
  const uint8_t rx_i2s_num = dual_bus ? this->rx_bus_.i2s_num : this->i2s_num_;
  const int tx_mclk_pin = dual_bus ? this->tx_bus_.mclk_pin : this->mclk_pin_;
  const int tx_bclk_pin = dual_bus ? this->tx_bus_.bclk_pin : this->bclk_pin_;
  const int tx_lrclk_pin = dual_bus ? this->tx_bus_.lrclk_pin : this->lrclk_pin_;
  const int tx_dout_pin = dual_bus ? this->tx_bus_.dout_pin : this->dout_pin_;
  const int rx_mclk_pin = dual_bus ? this->rx_bus_.mclk_pin : this->mclk_pin_;
  const int rx_bclk_pin = dual_bus ? this->rx_bus_.bclk_pin : this->bclk_pin_;
  const int rx_lrclk_pin = dual_bus ? this->rx_bus_.lrclk_pin : this->lrclk_pin_;
  const int rx_din_pin = dual_bus ? this->rx_bus_.din_pin : this->din_pin_;
#else
  const uint8_t tx_i2s_num = this->i2s_num_;
  const uint8_t rx_i2s_num = this->i2s_num_;
  const int tx_mclk_pin = this->mclk_pin_;
  const int tx_bclk_pin = this->bclk_pin_;
  const int tx_lrclk_pin = this->lrclk_pin_;
  const int tx_dout_pin = this->dout_pin_;
  const int rx_mclk_pin = this->mclk_pin_;
  const int rx_bclk_pin = this->bclk_pin_;
  const int rx_lrclk_pin = this->lrclk_pin_;
  const int rx_din_pin = this->din_pin_;
#endif
  (void) rx_mclk_pin;
  (void) rx_bclk_pin;
  (void) rx_lrclk_pin;

  const bool physical_tx = (tx_dout_pin >= 0);
  const bool need_rx = (rx_din_pin >= 0);
  // In ESP-IDF full-duplex STD/TDM, TX is the clock owner for RX. Create an
  // explicit clock-only TX channel even for mic-only configurations so RX has
  // BCLK/WS and the bus can be fully deleted when the audio task parks.
  const bool need_tx = physical_tx || need_rx;

  if (!physical_tx && !need_rx) {
    ESP_LOGE(TAG, "At least one of din_pin or dout_pin must be configured");
    this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
    return false;
  }

  // Channel configuration
  // Clock source: APLL for accurate clocking (ESP32 original only)
  i2s_clock_src_t clk_src = I2S_CLK_SRC_DEFAULT;
#ifdef I2S_CLK_SRC_APLL
  if (this->use_apll_)
    clk_src = I2S_CLK_SRC_APLL;
#endif
  i2s_mclk_multiple_t mclk_mult = get_mclk_multiple(this->mclk_multiple_);

  ESP_LOGD(TAG, "I2S driver config: TX=%s(port %u, physical=%s) RX=%s(port %u)", need_tx ? "yes" : "no", tx_i2s_num,
           physical_tx ? "yes" : "clock-only", need_rx ? "yes" : "no", rx_i2s_num);

  auto pin_or_nc = [](int pin) -> gpio_num_t { return pin >= 0 ? static_cast<gpio_num_t>(pin) : GPIO_NUM_NC; };

  uint32_t bytes_per_sample = (this->bits_per_sample_ > 16) ? 4 : 2;
  uint32_t tx_bytes_per_frame = this->num_channels_ * bytes_per_sample;
  uint32_t rx_bytes_per_frame = tx_bytes_per_frame;
  if (this->use_stereo_aec_ref_) {
    rx_bytes_per_frame = 2 * bytes_per_sample;
  }
#if SOC_I2S_SUPPORTS_TDM && defined(USE_ESP_AUDIO_STACK_TDM_BUS)
  if (this->use_tdm_bus_) {
    const uint32_t tdm_frame = this->tdm_total_slots_ * bytes_per_sample;
    tx_bytes_per_frame = tdm_frame;
    rx_bytes_per_frame = tdm_frame;
  }
#endif
  const uint32_t max_bytes_per_frame = std::max(tx_bytes_per_frame, rx_bytes_per_frame);
  uint32_t dma_desc_num = this->dma_desc_num_;
  uint32_t dma_frame_num = this->dma_frame_num_configured_
                               ? this->dma_frame_num_
                               : std::max<uint32_t>(64, (this->sample_rate_ * DMA_BUFFER_DURATION_MS) / 1000);
  uint32_t max_frames = 0;
  if (max_bytes_per_frame > 0) {
    max_frames = 4092 / max_bytes_per_frame;
    if (dma_frame_num > max_frames) {
      if (this->dma_frame_num_configured_) {
        ESP_LOGE(TAG, "dma_frame_num %u exceeds IDF DMA descriptor limit (%u frames for %u bytes/frame)",
                 (unsigned) dma_frame_num, (unsigned) max_frames, (unsigned) max_bytes_per_frame);
        this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
        return false;
      }
      dma_frame_num = max_frames;
    }
  }
  uint32_t logical_tx_frames = 256U * this->rate_conversion_ratio_;
#ifdef USE_AUDIO_PROCESSOR
  if (this->processor_ != nullptr) {
    const auto spec = this->processor_->frame_spec();
    if (spec.input_samples > 0) {
      logical_tx_frames = static_cast<uint32_t>(spec.input_samples) * this->rate_conversion_ratio_;
    }
  }
#endif
  if (logical_tx_frames > 0 && max_frames > 0) {
    const uint32_t aligned_frames = largest_divisor_at_most(logical_tx_frames, max_frames, 64);
    if (aligned_frames > 0 && aligned_frames != dma_frame_num) {
      if (this->dma_frame_num_configured_) {
        ESP_LOGW(TAG,
                 "Configured dma_frame_num=%u does not divide audio task TX frame (%u); "
                 "using %u so speaker playback callbacks stay in DMA lockstep",
                 (unsigned) dma_frame_num, (unsigned) logical_tx_frames, (unsigned) aligned_frames);
      } else {
        ESP_LOGD(TAG, "Aligning TX DMA to audio task frame: frames %u -> %u (logical=%u, max=%u)",
                 (unsigned) dma_frame_num, (unsigned) aligned_frames, (unsigned) logical_tx_frames,
                 (unsigned) max_frames);
      }
      dma_frame_num = aligned_frames;
    }
  }
#if SOC_I2S_SUPPORTS_TDM && defined(USE_ESP_AUDIO_STACK_TDM_BUS)
#ifdef USE_AUDIO_PROCESSOR
  if (this->use_tdm_bus_ && this->processor_ != nullptr && max_frames > 0) {
    const auto spec = this->processor_->frame_spec();
    const uint32_t processor_bus_frames = static_cast<uint32_t>(spec.input_samples) * this->rate_conversion_ratio_;
    if (processor_bus_frames > 0) {
      auto ceil_div_u32 = [](uint32_t num, uint32_t den) -> uint32_t { return den == 0 ? 0 : (num + den - 1) / den; };
      // The audio task reads/writes one processor frame per loop. Keep enough
      // DMA headroom for that full TDM frame plus margin; otherwise a 1024-sample
      // AFE quantum can underflow a short DMA queue even though the YAML compiles.
      // Do not change dma_frame_num here: it has already been selected as a
      // divisor of the logical TX frame so playback completion records stay in
      // lockstep with the I2S DMA callbacks.
      const uint32_t target_total_frames = ceil_div_u32(processor_bus_frames * 5U, 4U);
      if (target_total_frames > dma_desc_num * dma_frame_num) {
        const uint32_t old_desc = dma_desc_num;
        dma_desc_num = std::max(dma_desc_num, ceil_div_u32(target_total_frames, dma_frame_num));
        if (dma_desc_num > 16) {
          ESP_LOGE(TAG,
                   "TDM DMA queue too small for processor frame (%u bus frames, target %u); "
                   "need %u descriptors of %u frames, max supported is 16",
                   (unsigned) processor_bus_frames, (unsigned) target_total_frames, (unsigned) dma_desc_num,
                   (unsigned) dma_frame_num);
          this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
          return false;
        }
        ESP_LOGW(TAG,
                 "Raised TDM DMA queue for processor frame: desc %u->%u, frames %u "
                 "(processor=%u bus frames, target=%u)",
                 (unsigned) old_desc, (unsigned) dma_desc_num, (unsigned) dma_frame_num,
                 (unsigned) processor_bus_frames, (unsigned) target_total_frames);
      }
    }
  }
#endif
#endif

  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(static_cast<i2s_port_t>(dual_bus ? tx_i2s_num : this->i2s_num_),
                                 this->i2s_mode_secondary_ ? static_cast<i2s_role_t>(1) : I2S_ROLE_MASTER);
  chan_cfg.dma_desc_num = dma_desc_num;
  chan_cfg.dma_frame_num = dma_frame_num;
  chan_cfg.auto_clear = true;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
  chan_cfg.auto_clear_before_cb = false;
  chan_cfg.intr_priority = 0;
#endif

  esp_err_t err = ESP_OK;
#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  if (dual_bus) {
    if (need_tx) {
      chan_cfg.id = static_cast<i2s_port_t>(tx_i2s_num);
      err = i2s_new_channel(&chan_cfg, &this->tx_handle_, nullptr);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create TX I2S channel on port %u: %s", tx_i2s_num, esp_err_to_name(err));
        this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
        return false;
      }
    }
    if (need_rx) {
      chan_cfg.id = static_cast<i2s_port_t>(rx_i2s_num);
      err = i2s_new_channel(&chan_cfg, nullptr, &this->rx_handle_);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RX I2S channel on port %u: %s", rx_i2s_num, esp_err_to_name(err));
        this->deinit_i2s_();
        this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
        return false;
      }
    }
  } else {
#endif
    i2s_chan_handle_t *tx_ptr = need_tx ? &this->tx_handle_ : nullptr;
    i2s_chan_handle_t *rx_ptr = need_rx ? &this->rx_handle_ : nullptr;
    err = i2s_new_channel(&chan_cfg, tx_ptr, rx_ptr);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(err));
      this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
      return false;
    }
#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  }
#endif

#if SOC_I2S_SUPPORTS_TDM && defined(USE_ESP_AUDIO_STACK_TDM_BUS)
  if (this->use_tdm_bus_) {
    // ── TDM MODE: ES7210 multi-slot RX + ES8311 slot-0 TX ──
    // STEREO with 4 slots: DMA contains all 4 interleaved slots, BCLK/FS = 64.
    // ESP-IDF MONO only puts slot 0 in DMA; STEREO gives all active slots.
    // total_slot is derived from slot_mask (not slot_mode), so BCLK doesn't change.
    // ES8311 reads/writes slot 0 as standard I2S (first 16 bits after LRCLK edge).
    // DMA frame = tdm_total_slots × 2 bytes. At 4 slots, 256 frames = 2048 bytes/desc (< 4092 limit).
    i2s_tdm_slot_mask_t tdm_mask = I2S_TDM_SLOT0;
    for (int i = 1; i < this->tdm_total_slots_; i++)
      tdm_mask = static_cast<i2s_tdm_slot_mask_t>(tdm_mask | (I2S_TDM_SLOT0 << i));

    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg =
            {
                .sample_rate_hz = this->sample_rate_,
                .clk_src = clk_src,
                .ext_clk_freq_hz = 0,
                .mclk_multiple = mclk_mult,
            },
        .slot_cfg = get_tdm_slot_config(this->i2s_comm_fmt_, bit_width, I2S_SLOT_MODE_STEREO, tdm_mask),
        .gpio_cfg =
            {
                .mclk = pin_or_nc(this->mclk_pin_),
                .bclk = pin_or_nc(this->bclk_pin_),
                .ws = pin_or_nc(this->lrclk_pin_),
                .dout = pin_or_nc(this->dout_pin_),
                .din = pin_or_nc(this->din_pin_),
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };

    // Apply slot_bit_width override BEFORE init
    if (slot_bw != I2S_SLOT_BIT_WIDTH_AUTO) {
      tdm_cfg.slot_cfg.slot_bit_width = slot_bw;
    }

    if (this->tx_handle_) {
      err = i2s_channel_init_tdm_mode(this->tx_handle_, &tdm_cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init TX TDM channel: %s", esp_err_to_name(err));
        this->deinit_i2s_();
        this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
        return false;
      }
      ESP_LOGD(TAG, "TX TDM channel initialized");
    }
    if (this->rx_handle_) {
      err = i2s_channel_init_tdm_mode(this->rx_handle_, &tdm_cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init RX TDM channel: %s", esp_err_to_name(err));
        this->deinit_i2s_();
        this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
        return false;
      }
      ESP_LOGD(TAG, "RX TDM channel initialized");
    }

    if (this->tdm_second_mic_slot_ >= 0) {
      ESP_LOGD(TAG, "TDM mode: %d slots, mic_slots=[%d,%d], ref_slot=%d, mask=0x%x", this->tdm_total_slots_,
               this->tdm_mic_slot_, this->tdm_second_mic_slot_, this->tdm_ref_slot_, (unsigned) tdm_mask);
    } else {
      ESP_LOGD(TAG, "TDM mode: %d slots, mic_slot=%d, ref_slot=%d, mask=0x%x", this->tdm_total_slots_,
               this->tdm_mic_slot_, this->tdm_ref_slot_, (unsigned) tdm_mask);
    }
  } else
#endif  // SOC_I2S_SUPPORTS_TDM
  {
    // ── STANDARD MODE ──
    i2s_slot_mode_t tx_slot_mode = (this->num_channels_ == 2) ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO;
    i2s_std_config_t tx_cfg = {
        .clk_cfg =
            {
                .sample_rate_hz = this->sample_rate_,
                .clk_src = clk_src,
                .mclk_multiple = mclk_mult,
            },
        .slot_cfg = get_std_slot_config(this->i2s_comm_fmt_, bit_width, tx_slot_mode),
        .gpio_cfg =
            {
                .mclk = pin_or_nc(tx_mclk_pin),
                .bclk = pin_or_nc(tx_bclk_pin),
                .ws = pin_or_nc(tx_lrclk_pin),
                .dout = pin_or_nc(tx_dout_pin),
                .din = dual_bus ? GPIO_NUM_NC : pin_or_nc(rx_din_pin),
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };
    tx_cfg.slot_cfg.slot_mask = (this->num_channels_ == 2)
                                    ? I2S_STD_SLOT_BOTH
                                    : (this->tx_slot_right_ ? I2S_STD_SLOT_RIGHT : I2S_STD_SLOT_LEFT);
    // Apply slot_bit_width override
    if (slot_bw != I2S_SLOT_BIT_WIDTH_AUTO) {
      tx_cfg.slot_cfg.slot_bit_width = slot_bw;
    }

    // RX configuration - always independent of TX num_channels
    i2s_std_config_t rx_cfg = tx_cfg;
    if (dual_bus) {
      rx_cfg.gpio_cfg.mclk = pin_or_nc(rx_mclk_pin);
      rx_cfg.gpio_cfg.bclk = pin_or_nc(rx_bclk_pin);
      rx_cfg.gpio_cfg.ws = pin_or_nc(rx_lrclk_pin);
      rx_cfg.gpio_cfg.dout = GPIO_NUM_NC;
      rx_cfg.gpio_cfg.din = pin_or_nc(rx_din_pin);
    }
    if (this->use_stereo_aec_ref_ || this->rx_slot_mode_stereo_) {
      rx_cfg.slot_cfg = get_std_slot_config(this->i2s_comm_fmt_, bit_width, I2S_SLOT_MODE_STEREO);
      rx_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
      ESP_LOGD(TAG, "RX configured as STEREO (%s)", this->use_stereo_aec_ref_ ? "AEC reference" : "mic channel select");
    } else {
      rx_cfg.slot_cfg = get_std_slot_config(this->i2s_comm_fmt_, bit_width, I2S_SLOT_MODE_MONO);
      rx_cfg.slot_cfg.slot_mask = this->mic_channel_right_ ? I2S_STD_SLOT_RIGHT : I2S_STD_SLOT_LEFT;
    }
    // Apply slot_bit_width override to RX
    if (slot_bw != I2S_SLOT_BIT_WIDTH_AUTO) {
      rx_cfg.slot_cfg.slot_bit_width = slot_bw;
    }

    if (this->tx_handle_) {
      err = i2s_channel_init_std_mode(this->tx_handle_, &tx_cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init TX I2S channel: %s", esp_err_to_name(err));
        this->deinit_i2s_();
        this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
        return false;
      }
      ESP_LOGD(TAG, "TX channel initialized");
    }

    if (this->rx_handle_) {
      err = i2s_channel_init_std_mode(this->rx_handle_, &rx_cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init RX I2S channel: %s", esp_err_to_name(err));
        this->deinit_i2s_();
        this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
        return false;
      }
      ESP_LOGD(TAG, "RX channel initialized (%s)", this->use_stereo_aec_ref_ ? "stereo" : "mono");
    }
  }

  this->set_i2s_hardware_state_(I2SHardwareState::READY);
  this->tx_completion_dma_frames_ = need_tx ? dma_frame_num : 0;
  this->tx_completion_bytes_per_frame_ = need_tx ? tx_bytes_per_frame : 0;
  this->tx_completion_dma_buffer_bytes_ =
      need_tx ? static_cast<size_t>(dma_frame_num) * static_cast<size_t>(tx_bytes_per_frame) : 0;
  this->tx_completion_queue_size_ = need_tx ? static_cast<size_t>(dma_desc_num) * 2U : 0;
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  if (!this->setup_codec_backend_(clk_src)) {
    ESP_LOGE(TAG, "Failed to prepare esp_codec_dev backend");
    this->deinit_i2s_();
    this->set_i2s_hardware_state_(I2SHardwareState::ERROR);
    return false;
  }
#endif
  this->log_memory_snapshot_("after_i2s_prepare");
  ESP_LOGI(TAG, "ESP audio stack I2S prepared through esp_driver_i2s (%s, dma_desc=%u, dma_frames=%u)",
           this->use_tdm_bus_ ? "TDM" : "standard", static_cast<unsigned>(dma_desc_num),
           static_cast<unsigned>(dma_frame_num));
  return true;
}

#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
CodecDevBackend::SampleConfig ESPAudioStack::make_tx_sample_config_() const {
  CodecDevBackend::SampleConfig cfg;
  cfg.sample_rate = this->sample_rate_;
  cfg.bits_per_sample = this->bits_per_sample_;
  cfg.mclk_multiple = this->mclk_multiple_;
#if SOC_I2S_SUPPORTS_TDM && defined(USE_ESP_AUDIO_STACK_TDM_BUS)
  if (this->use_tdm_bus_) {
    cfg.channels = this->tdm_total_slots_;
    cfg.channel_mask = 0;
    for (uint8_t i = 0; i < this->tdm_total_slots_; i++) {
      cfg.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(i);
    }
    return cfg;
  }
#endif
  if (this->num_channels_ == 2) {
    cfg.channels = 2;
    cfg.channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0) | ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
  } else {
    cfg.channels = 2;
    cfg.channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(this->tx_slot_right_ ? 1 : 0);
  }
  return cfg;
}

CodecDevBackend::SampleConfig ESPAudioStack::make_rx_sample_config_() const {
  CodecDevBackend::SampleConfig cfg;
  cfg.sample_rate = this->sample_rate_;
  cfg.bits_per_sample = this->bits_per_sample_;
  cfg.mclk_multiple = this->mclk_multiple_;
#if SOC_I2S_SUPPORTS_TDM && defined(USE_ESP_AUDIO_STACK_TDM_BUS)
  if (this->use_tdm_bus_) {
    cfg.channels = this->tdm_total_slots_;
    cfg.channel_mask = 0;
    for (uint8_t i = 0; i < this->tdm_total_slots_; i++) {
      cfg.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(i);
    }
    return cfg;
  }
#endif
  if (this->use_stereo_aec_ref_) {
    cfg.channels = 2;
    cfg.channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0) | ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
  } else {
    cfg.channels = 2;
    cfg.channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(this->mic_channel_right_ ? 1 : 0);
  }
  return cfg;
}

bool ESPAudioStack::setup_codec_backend_(i2s_clock_src_t clk_src) {
#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  const uint8_t tx_i2s_num = this->dual_i2s_bus_ ? this->tx_bus_.i2s_num : this->i2s_num_;
  const uint8_t rx_i2s_num = this->dual_i2s_bus_ ? this->rx_bus_.i2s_num : this->i2s_num_;
#else
  const uint8_t tx_i2s_num = this->i2s_num_;
  const uint8_t rx_i2s_num = this->i2s_num_;
#endif
  return this->codec_backend_.setup(tx_i2s_num, rx_i2s_num, this->tx_handle_, this->rx_handle_, clk_src,
                                    this->mclk_multiple_);
}
#endif

bool ESPAudioStack::enable_i2s_channels_() {
  auto state = static_cast<I2SHardwareState>(this->i2s_hardware_state_.load(std::memory_order_relaxed));
  if (state == I2SHardwareState::RUNNING) {
    return true;
  }
  if (state == I2SHardwareState::ERROR) {
    ESP_LOGE(TAG, "Cannot enable I2S from error state");
    return false;
  }
  if (!this->prepare_i2s_channels_()) {
    return false;
  }

  if (this->tx_handle_ != nullptr) {
    if (!this->prepare_tx_completion_tracking_()) {
      this->deinit_i2s_();
      return false;
    }
    const i2s_event_callbacks_t callbacks = {.on_sent = ESPAudioStack::tx_on_sent_callback};
    const esp_err_t cb_err = i2s_channel_register_event_callback(this->tx_handle_, &callbacks, this);
    if (cb_err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to register TX completion callback: %s", esp_err_to_name(cb_err));
      this->deinit_i2s_();
      return false;
    }
  }

#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  if (!this->prime_tx_completion_records_(false)) {
    this->deinit_i2s_();
    return false;
  }
  auto tx_cfg = this->make_tx_sample_config_();
  auto rx_cfg = this->make_rx_sample_config_();
  if (!this->codec_backend_.open(this->tx_handle_ ? &tx_cfg : nullptr, this->rx_handle_ ? &rx_cfg : nullptr)) {
    ESP_LOGE(TAG, "Failed to open esp_codec_dev backend");
    this->deinit_i2s_();
    return false;
  }
  this->codec_backend_.set_output_volume(this->master_volume_linear_.load(std::memory_order_relaxed));
  this->codec_backend_.set_output_mute(false);
#else
  if (this->tx_handle_) {
    if (!this->prime_tx_completion_records_(true)) {
      this->deinit_i2s_();
      return false;
    }
    esp_err_t err = i2s_channel_enable(this->tx_handle_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to enable TX I2S channel: %s", esp_err_to_name(err));
      this->deinit_i2s_();
      return false;
    }
  }
  if (this->rx_handle_) {
    esp_err_t err = i2s_channel_enable(this->rx_handle_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to enable RX I2S channel: %s", esp_err_to_name(err));
      if (this->tx_handle_) {
        i2s_channel_disable(this->tx_handle_);
      }
      this->deinit_i2s_();
      return false;
    }
  }
#endif
  this->set_i2s_hardware_state_(I2SHardwareState::RUNNING);
  this->log_memory_snapshot_("after_i2s_enable");
  ESP_LOGI(TAG, "ESP audio stack running (%s)", this->use_tdm_bus_ ? "TDM" : "standard");
  return true;
}

bool ESPAudioStack::init_audio_stack_() { return this->enable_i2s_channels_(); }

void ESPAudioStack::close_audio_io_() {
  auto state = static_cast<I2SHardwareState>(this->i2s_hardware_state_.load(std::memory_order_relaxed));
  if (state != I2SHardwareState::RUNNING && state != I2SHardwareState::STOPPING) {
    return;
  }
  this->set_i2s_hardware_state_(I2SHardwareState::STOPPING);
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  this->codec_backend_.close();
#else
  if (this->rx_handle_) {
    esp_err_t err = i2s_channel_disable(this->rx_handle_);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "Failed to disable RX I2S channel: %s", esp_err_to_name(err));
    }
  }
  if (this->tx_handle_) {
    esp_err_t err = i2s_channel_disable(this->tx_handle_);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "Failed to disable TX I2S channel: %s", esp_err_to_name(err));
    }
  }
#endif
  this->set_i2s_hardware_state_(I2SHardwareState::READY);
  this->reset_tx_completion_tracking_();
  this->log_memory_snapshot_("after_i2s_disable");
}

void ESPAudioStack::deinit_i2s_() {
  this->close_audio_io_();
  this->reset_tx_completion_tracking_();
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  this->codec_backend_.teardown();
#endif
  if (this->tx_handle_) {
    i2s_del_channel(this->tx_handle_);
    this->tx_handle_ = nullptr;
  }
  if (this->rx_handle_) {
    i2s_del_channel(this->rx_handle_);
    this->rx_handle_ = nullptr;
  }
  this->set_i2s_hardware_state_(I2SHardwareState::UNPREPARED);
  ESP_LOGI(TAG, "I2S deinitialized");
}

bool IRAM_ATTR ESPAudioStack::tx_on_sent_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
  auto *self = static_cast<ESPAudioStack *>(user_ctx);
  if (self == nullptr || self->tx_completion_event_queue_ == nullptr) {
    return false;
  }
  const int64_t now = esp_timer_get_time();
  BaseType_t need_yield1 = pdFALSE;
  BaseType_t need_yield2 = pdFALSE;
  if (xQueueIsQueueFullFromISR(self->tx_completion_event_queue_)) {
    int64_t dummy = 0;
    xQueueReceiveFromISR(self->tx_completion_event_queue_, &dummy, &need_yield1);
    self->tx_completion_desync_ = true;
  }
  xQueueSendToBackFromISR(self->tx_completion_event_queue_, &now, &need_yield2);
  (void) handle;
  (void) event;
  return need_yield1 || need_yield2;
}

bool ESPAudioStack::prepare_tx_completion_tracking_() {
  if (this->tx_handle_ == nullptr) {
    return true;
  }
  if (this->tx_completion_dma_buffer_bytes_ == 0 || this->tx_completion_queue_size_ == 0) {
    ESP_LOGE(TAG, "TX completion tracking missing DMA sizing");
    return false;
  }
  if (this->tx_completion_event_queue_ != nullptr) {
    vQueueDelete(this->tx_completion_event_queue_);
    this->tx_completion_event_queue_ = nullptr;
  }
  if (this->tx_completion_record_queue_ != nullptr) {
    vQueueDelete(this->tx_completion_record_queue_);
    this->tx_completion_record_queue_ = nullptr;
  }
  this->tx_completion_event_queue_ = xQueueCreate(this->tx_completion_queue_size_, sizeof(int64_t));
  this->tx_completion_record_queue_ = xQueueCreate(this->tx_completion_queue_size_, sizeof(TxCompletionRecord));
  if (this->tx_completion_event_queue_ == nullptr || this->tx_completion_record_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate TX completion queues");
    return false;
  }
  if (this->tx_completion_silence_bytes_ < this->tx_completion_dma_buffer_bytes_) {
    if (this->tx_completion_silence_ != nullptr) {
      heap_caps_free(this->tx_completion_silence_);
      this->tx_completion_silence_ = nullptr;
      this->tx_completion_silence_bytes_ = 0;
    }
    this->tx_completion_silence_ = static_cast<uint8_t *>(
        heap_caps_calloc(1, this->tx_completion_dma_buffer_bytes_, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    this->tx_completion_silence_bytes_ =
        this->tx_completion_silence_ != nullptr ? this->tx_completion_dma_buffer_bytes_ : 0;
  } else if (this->tx_completion_silence_ != nullptr) {
    memset(this->tx_completion_silence_, 0, this->tx_completion_dma_buffer_bytes_);
  }
  if (this->tx_completion_silence_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate TX DMA silence buffer (%u bytes)",
             (unsigned) this->tx_completion_dma_buffer_bytes_);
    return false;
  }
  this->tx_completion_desync_ = false;
  this->tx_completion_pending_real_records_.store(0, std::memory_order_release);
  return true;
}

bool ESPAudioStack::prime_tx_completion_records_(bool preload_dma) {
  if (this->tx_handle_ == nullptr) {
    return true;
  }
  const size_t desc_count = this->tx_completion_queue_size_ / 2U;
  for (size_t i = 0; i < desc_count; i++) {
    if (preload_dma) {
      size_t loaded = 0;
      const esp_err_t err = i2s_channel_preload_data(this->tx_handle_, this->tx_completion_silence_,
                                                     this->tx_completion_dma_buffer_bytes_, &loaded);
      if (err != ESP_OK || loaded != this->tx_completion_dma_buffer_bytes_) {
        ESP_LOGE(TAG, "Failed to preload TX DMA silence %u/%u: %s loaded=%u", (unsigned) (i + 1U),
                 (unsigned) desc_count, esp_err_to_name(err), (unsigned) loaded);
        return false;
      }
    }
    TxCompletionRecord zero{};
    if (xQueueSend(this->tx_completion_record_queue_, &zero, 0) != pdTRUE) {
      ESP_LOGE(TAG, "Failed to queue TX priming completion record");
      return false;
    }
  }
  return true;
}

void ESPAudioStack::reset_tx_completion_tracking_() {
  this->tx_completion_desync_ = false;
  this->tx_completion_pending_real_records_.store(0, std::memory_order_release);
  if (this->tx_completion_event_queue_ != nullptr) {
    xQueueReset(this->tx_completion_event_queue_);
  }
  if (this->tx_completion_record_queue_ != nullptr) {
    xQueueReset(this->tx_completion_record_queue_);
  }
}

void ESPAudioStack::dispatch_speaker_output_callbacks_(uint32_t frames, int64_t timestamp) {
  for (size_t i = 0; i < this->speaker_output_callback_count_; i++) {
    const auto &slot = this->speaker_output_callbacks_[i];
    if (slot.fn != nullptr) {
      slot.fn(slot.ctx, frames, timestamp);
    }
  }
}

void ESPAudioStack::drain_tx_completion_events_() {
  if (this->tx_completion_event_queue_ == nullptr || this->tx_completion_record_queue_ == nullptr) {
    return;
  }
  if (this->tx_completion_desync_) {
    ESP_LOGE(TAG, "TX completion event queue overflowed; stopping audio stack to restore lockstep");
    this->has_i2s_error_.store(true, std::memory_order_relaxed);
    this->audio_stack_running_.store(false, std::memory_order_relaxed);
    return;
  }
  int64_t timestamp = 0;
  while (xQueueReceive(this->tx_completion_event_queue_, &timestamp, 0) == pdTRUE) {
    TxCompletionRecord record{};
    if (xQueueReceive(this->tx_completion_record_queue_, &record, 0) != pdTRUE) {
      if (this->tx_completion_pending_real_records_.load(std::memory_order_acquire) == 0) {
        // esp_codec_dev_open()/I2S channel reconfiguration can emit initial TX
        // completion callbacks before the audio task has queued clock-only or
        // speaker records. With no real speaker frames pending, those callbacks
        // are driver priming noise and must not stop the full-duplex mic path.
        continue;
      }
      ESP_LOGE(TAG, "TX completion event without matching write record; stopping audio stack");
      this->has_i2s_error_.store(true, std::memory_order_relaxed);
      this->audio_stack_running_.store(false, std::memory_order_relaxed);
      return;
    }
    if (record.real_frames == 0) {
      continue;
    }
    if (this->tx_completion_pending_real_records_.load(std::memory_order_acquire) > 0) {
      this->tx_completion_pending_real_records_.fetch_sub(1, std::memory_order_acq_rel);
    }
    if (this->speaker_output_callback_count_ == 0) {
      continue;
    }
    const int64_t adjusted_timestamp = timestamp - static_cast<int64_t>(record.trailing_silence_frames) * 1000000LL /
                                                       static_cast<int64_t>(this->sample_rate_);
    this->dispatch_speaker_output_callbacks_(record.real_frames, adjusted_timestamp);
  }
}

bool ESPAudioStack::queue_tx_completion_record_(const TxCompletionRecord &record) {
  if (this->tx_completion_record_queue_ == nullptr) {
    return true;
  }
  if (xQueueSend(this->tx_completion_record_queue_, &record, 0) != pdTRUE) {
    ESP_LOGE(TAG, "TX completion record queue full; stopping audio stack");
    this->has_i2s_error_.store(true, std::memory_order_relaxed);
    this->audio_stack_running_.store(false, std::memory_order_relaxed);
    return false;
  }
  if (record.real_frames > 0) {
    this->tx_completion_pending_real_records_.fetch_add(1, std::memory_order_acq_rel);
  }
  return true;
}

bool ESPAudioStack::wait_audio_task_state_(bool idle, uint32_t timeout_ms) {
  if (this->audio_task_idle_.load(std::memory_order_relaxed) == idle) {
    return true;
  }
  TaskHandle_t waiter = xTaskGetCurrentTaskHandle();
  ulTaskNotifyTake(pdTRUE, 0);
  this->audio_state_waiter_.store(waiter, std::memory_order_release);
  if (this->audio_task_idle_.load(std::memory_order_relaxed) == idle) {
    this->audio_state_waiter_.store(nullptr, std::memory_order_release);
    return true;
  }
  const bool ok = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms)) > 0 ||
                  this->audio_task_idle_.load(std::memory_order_relaxed) == idle;
  this->audio_state_waiter_.store(nullptr, std::memory_order_release);
  return ok;
}

void ESPAudioStack::start() {
  if (this->audio_stack_running_.load(std::memory_order_relaxed)) {
    return;
  }

  // Complete any deferred close from the previous session before reopening.
  // Reusing codec/I2S handles that the audio task has already parked during a
  // stop/start call cycle can leave TX in a half-closed state on the next
  // playback or an active call.
  if (this->teardown_pending_.load(std::memory_order_relaxed)) {
    if (this->wait_audio_task_idle_(250)) {
      this->deinit_i2s_();
      this->teardown_pending_.store(false, std::memory_order_relaxed);
      ESP_LOGI(TAG, "Completed pending audio stack teardown before restart");
    } else {
      ESP_LOGW(TAG, "Audio stack restart refused while teardown is still pending");
      return;
    }
  }

  ESP_LOGI(TAG, "Starting audio stack...");

  // setup() normally pre-creates the task. Manually constructed test instances
  // can still enter through start(), so create the task here if needed.
  if (this->audio_task_handle_ == nullptr) {
    const BaseType_t core = this->task_core_ >= 0 ? this->task_core_ : tskNO_AFFINITY;
    if (!start_pinned_task(audio_task, "audio_stack", this->task_stack_size_, this, this->task_priority_, core,
                           this->audio_task_stack_in_psram_, TAG, &this->audio_task_handle_, &this->audio_task_tcb_,
                           &this->audio_task_stack_)) {
      this->has_i2s_error_.store(true, std::memory_order_relaxed);
      return;
    }
  }

  // I2S allocation happens here, not in setup(): start() owns both driver
  // channel creation and codec open while stop() tears the bus back down.
  this->request_audio_preallocation_();
  if (!this->enable_i2s_channels_()) {
    ESP_LOGE(TAG, "Failed to start I2S");
    return;
  }

  this->has_i2s_error_.store(false, std::memory_order_relaxed);
  // start_speaker()/stop_speaker() own speaker_running_. A mic-only start
  // still writes silence to TX to keep the full-duplex bus clocked, but it must not
  // make the component look like active playback; otherwise the last mic
  // consumer leaving would fail to park the pipeline.

  // Wake the permanent audio task (created once in setup()).
  this->audio_stack_running_.store(true, std::memory_order_relaxed);
  if (this->audio_task_handle_ != nullptr) {
    xTaskNotifyGive(this->audio_task_handle_);
  }

#ifdef USE_AUDIO_PROCESSOR
  if (this->use_stereo_aec_ref_) {
    ESP_LOGD(TAG, "ES8311 digital feedback - reference is sample-aligned");
  }
  if (this->use_tdm_ref_) {
    ESP_LOGD(TAG, "TDM hardware reference - slot %u is echo ref", this->tdm_ref_slot_);
  }
  if (this->processor_ != nullptr && this->has_mic_consumers_.load(std::memory_order_relaxed)) {
    this->wait_audio_task_active_(50);
    this->processor_->set_processing_active(true);
  }
#endif

  this->start_trigger_.trigger();
  ESP_LOGI(TAG, "Audio stack started");
}

void ESPAudioStack::stop() {
  if (!this->audio_stack_running_.load(std::memory_order_relaxed)) {
    return;
  }

  ESP_LOGI(TAG, "Stopping audio stack (deferred)");

  // Consumers stay registered across stop()/start() so the mic path is
  // reconnected automatically after an internal restart (frame_spec change).
  if (this->speaker_running_.exchange(false, std::memory_order_relaxed)) {
    this->speaker_idle_trigger_.trigger();
    this->update_runtime_state_();
  }
#ifdef USE_AUDIO_PROCESSOR
  if (this->processor_ != nullptr) {
    this->processor_->set_processing_active(false);
  }
#endif
  this->audio_stack_running_.store(false, std::memory_order_relaxed);
  this->idle_trigger_.trigger();

  // Defer I2S deletion to loop(): polling audio_task_idle_ here would block
  // the main task for up to 600 ms (often >60 ms), starving network/UI/LVGL.
  // loop() picks this up on the next tick once the audio task has parked.
  this->teardown_pending_.store(true, std::memory_order_relaxed);
}

bool ESPAudioStack::stop_and_wait(uint32_t timeout_ms) {
  this->stop();

  if (!this->wait_audio_task_idle_(timeout_ms)) {
    ESP_LOGW(TAG, "Timed out waiting for audio task to stop before maintenance");
    return false;
  }

  if (this->teardown_pending_.load(std::memory_order_relaxed)) {
    this->deinit_i2s_();
    this->teardown_pending_.store(false, std::memory_order_relaxed);
    ESP_LOGI(TAG, "Audio stack stopped synchronously");
  }
  return true;
}

bool ESPAudioStack::register_mic_consumer(void *token) {
  bool needs_start = false;
  size_t count_after = 0;
  bool first_consumer = false;
  bool full = false;
  if (this->mic_consumers_mutex_ == nullptr)
    return false;
  {
    ScopedLock lock(this->mic_consumers_mutex_);
    // Already registered?
    for (size_t i = 0; i < this->mic_consumer_count_; i++) {
      if (this->mic_consumers_[i] == token)
        return true;
    }
    if (this->mic_consumer_count_ >= MAX_LISTENERS) {
      full = true;
    } else {
      first_consumer = (this->mic_consumer_count_ == 0);
      this->mic_consumers_[this->mic_consumer_count_++] = token;
      count_after = this->mic_consumer_count_;
      this->has_mic_consumers_.store(true, std::memory_order_relaxed);
      needs_start = !this->audio_stack_running_.load(std::memory_order_relaxed);
    }
  }
  if (full) {
    ESP_LOGW(TAG, "Mic consumer registry full (max=%u), refusing token=%p", (unsigned) MAX_LISTENERS, token);
    return false;
  }
  if (first_consumer) {
    ESP_LOGI(TAG, "Mic consumer registered (token=%p), mic path active (consumers=%zu)", token, count_after);
    this->mic_start_trigger_.trigger();
    // If the stack is already running, wake the processor immediately. If this
    // consumer is also starting the stack, start() will wake the audio task
    // first and then enable the processor so AFE has input frames available.
#ifdef USE_AUDIO_PROCESSOR
    if (!needs_start && this->processor_ != nullptr) {
      this->processor_->set_processing_active(true);
    }
#endif
  } else {
    ESP_LOGD(TAG, "Mic consumer registered (token=%p, consumers=%zu)", token, count_after);
  }
  if (needs_start) {
    this->start();
  }
  if (first_consumer) {
    this->update_runtime_state_();
  }
  return true;
}

void ESPAudioStack::unregister_mic_consumer(void *token) {
  size_t count_after = 0;
  bool removed = false;
  bool last_consumer_gone = false;
  if (this->mic_consumers_mutex_ == nullptr)
    return;
  {
    ScopedLock lock(this->mic_consumers_mutex_);
    for (size_t i = 0; i < this->mic_consumer_count_; i++) {
      if (this->mic_consumers_[i] == token) {
        // Swap-and-pop: order in the array is not meaningful, swap with last.
        this->mic_consumers_[i] = this->mic_consumers_[this->mic_consumer_count_ - 1];
        this->mic_consumers_[this->mic_consumer_count_ - 1] = nullptr;
        this->mic_consumer_count_--;
        removed = true;
        break;
      }
    }
    count_after = this->mic_consumer_count_;
    last_consumer_gone = removed && this->mic_consumer_count_ == 0;
    this->has_mic_consumers_.store(this->mic_consumer_count_ != 0, std::memory_order_relaxed);
  }
  if (!removed) {
    return;
  }
  if (last_consumer_gone) {
    ESP_LOGI(TAG, "Last mic consumer removed (token=%p), mic path idle", token);
    this->mic_idle_trigger_.trigger();
    // Tell the audio processor it can suspend background work until a new
    // consumer arrives. Without this hint esp_afe's GMF pipeline/task (and
    // the esp-sr internal worker on Core 1) keep cycling on every frame
    // even when nobody is listening, which on spotpear-ball-v2 was monopolising
    // CPU1 long enough to trip the loopTask 30s watchdog on HA restart.
#ifdef USE_AUDIO_PROCESSOR
    if (this->processor_ != nullptr) {
      this->processor_->set_processing_active(false);
    }
#endif
    // If no playback either, park the audio task and delete I2S channels once
    // the task is idle.
    if (!this->speaker_running_.load(std::memory_order_relaxed)) {
      ESP_LOGI(TAG, "Audio stack going idle (no consumers, no playback)");
      this->stop();
    }
    this->update_runtime_state_();
  } else {
    ESP_LOGD(TAG, "Mic consumer unregistered (token=%p, consumers=%zu)", token, count_after);
  }
}

void ESPAudioStack::start_speaker() {
  if (!this->audio_stack_running_.load(std::memory_order_relaxed)) {
    this->start();
  }
  if (!this->audio_stack_running_.load(std::memory_order_relaxed)) {
    ESP_LOGW(TAG, "Speaker start refused: audio stack did not enter RUNNING");
    return;
  }
  if (!this->speaker_running_.load(std::memory_order_relaxed)) {
#ifdef USE_ESP_AUDIO_STACK_MONO_REF
    this->direct_aec_ref_valid_ = false;
#endif
#ifdef USE_ESP_AUDIO_STACK_TDM_REF_DIAGNOSTIC
    this->tdm_ref_silent_frames_.store(0, std::memory_order_relaxed);
#endif
  }
  if (!this->speaker_running_.exchange(true, std::memory_order_relaxed)) {
    this->speaker_start_trigger_.trigger();
    this->update_runtime_state_();
  }
}

void ESPAudioStack::stop_speaker() {
  if (this->speaker_running_.exchange(false, std::memory_order_relaxed)) {
    this->speaker_idle_trigger_.trigger();
    this->update_runtime_state_();
  }
  // Request audio task to reset ring buffers (avoids concurrent access).
  this->request_speaker_reset_.store(true, std::memory_order_relaxed);
  // If no mic consumers either, tear down the audio stack pipeline. This signals
  // the audio processor (e.g. AFE) it can suspend its workers and parks the
  // audio task; channels stay configured for fast wake on the next start.
  if (!this->has_mic_consumers_.load(std::memory_order_relaxed)) {
    ESP_LOGI(TAG, "Audio stack going idle (speaker stopped, no mic consumers)");
#ifdef USE_AUDIO_PROCESSOR
    if (this->processor_ != nullptr) {
      this->processor_->set_processing_active(false);
    }
#endif
    this->stop();
  }
}

size_t ESPAudioStack::play(const uint8_t *data, size_t len, TickType_t ticks_to_wait) {
  if (!this->speaker_buffer_) {
    return 0;
  }

  // Data arrives at bus rate (e.g. 48kHz from mixer/resampler). Partial writes
  // are allowed because upstream ESPHome sources retry unwritten tails, but a
  // partial must never split an interleaved PCM frame.
  const size_t frame_bytes = sizeof(int16_t) * static_cast<size_t>(this->get_speaker_channels());
  if (frame_bytes == 0) {
    return 0;
  }
  const size_t aligned_len = len - (len % frame_bytes);
  if (aligned_len == 0) {
    return 0;
  }
  size_t written = this->speaker_buffer_->write_without_replacement((void *) data, aligned_len, ticks_to_wait, false);

  if (written > 0) {
    this->last_speaker_audio_ms_.store(millis(), std::memory_order_relaxed);
  }
  return written;
}

size_t ESPAudioStack::get_speaker_buffer_available() const {
  if (!this->speaker_buffer_)
    return 0;
  return this->speaker_buffer_->available();
}

size_t ESPAudioStack::get_speaker_buffer_size() const { return this->speaker_buffer_size_; }

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32
