#include "esp_audio_stack.h"

#ifdef USE_ESP32

#if __has_include(<esp_ae_alc.h>) && __has_include(<esp_ae_ch_cvt.h>) && __has_include(<gain.h>)
#include <esp_ae_alc.h>
#include <esp_ae_ch_cvt.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <gain.h>
#ifdef USE_ESP_AUDIO_STACK_32BIT
#if __has_include(<esp_ae_bit_cvt.h>)
#include <esp_ae_bit_cvt.h>
#endif
#endif
#if defined(USE_ESP_AUDIO_STACK_TDM_BUS) || defined(USE_ESP_AUDIO_STACK_STEREO_TX)
#if __has_include(<esp_ae_data_weaver.h>)
#include <esp_ae_data_weaver.h>
#endif
#endif
#endif
#include <algorithm>
#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_AUDIO_PROCESSOR
#include "audio_core_processor.h"
#endif
#include "audio_core_audio_utils.h"
#include "audio_core_log_utils.h"
#ifdef USE_ESP_AUDIO_STACK_RING_REF
#include "audio_core_ring_buffer_caps.h"
#endif

namespace esphome::esp_audio_stack {

#if __has_include(<esp_ae_alc.h>) && __has_include(<esp_ae_ch_cvt.h>) && __has_include(<gain.h>)

static const char *const TAG = "audio_stack";

// Default frame size when no AudioProcessor is attached.
static const size_t DEFAULT_FRAME_SIZE = 256;
static constexpr size_t AUDIO_BUFFER_ALIGN = 16;

static inline bool alloc_i16_buffer(int16_t **buffer, size_t *buffer_bytes, size_t bytes, uint32_t caps, bool aligned) {
  if (bytes == 0) {
    return true;
  }
  *buffer = static_cast<int16_t *>(aligned ? heap_caps_aligned_alloc(AUDIO_BUFFER_ALIGN, bytes, caps)
                                           : heap_caps_malloc(bytes, caps));
  if (*buffer == nullptr) {
    return false;
  }
  if (buffer_bytes != nullptr) {
    *buffer_bytes = bytes;
  }
  return true;
}

static inline void free_i16_buffer(int16_t **buffer, size_t *buffer_bytes = nullptr) {
  if (*buffer != nullptr) {
    heap_caps_free(*buffer);
    *buffer = nullptr;
  }
  if (buffer_bytes != nullptr) {
    *buffer_bytes = 0;
  }
}

size_t ESPAudioStack::get_mic_callback_buffer_size() const {
  // ESPHome microphone callbacks are copied from the realtime audio task, so
  // the backing vector must already cover later processor frame_spec bumps.
  // GMF AFE on P4/S3 can publish 1024-sample output frames after setup.
  size_t samples = std::max<size_t>(DEFAULT_FRAME_SIZE, 1024);
#ifdef USE_AUDIO_PROCESSOR
  const esp_audio_stack::FrameSpec default_spec{};
  samples = std::max(samples, std::max(default_spec.input_samples, default_spec.output_samples));
  if (this->processor_ != nullptr) {
    const auto spec = this->processor_->frame_spec();
    if (spec.input_samples > 0) {
      samples = std::max(samples, spec.input_samples);
    }
    if (spec.output_samples > 0) {
      samples = std::max(samples, spec.output_samples);
    }
  }
#endif
  return samples * sizeof(int16_t);
}

void ESPAudioStack::release_audio_buffers_() {
  free_i16_buffer(&this->prealloc_rx_buffer_, &this->prealloc_rx_buffer_bytes_);
  free_i16_buffer(&this->prealloc_mic_buffer_, &this->prealloc_mic_buffer_bytes_);
  free_i16_buffer(&this->prealloc_processor_mic_buffer_, &this->prealloc_processor_mic_buffer_bytes_);
  free_i16_buffer(&this->prealloc_spk_buffer_, &this->prealloc_spk_buffer_bytes_);
  free_i16_buffer(&this->prealloc_spk_ref_buffer_, &this->prealloc_spk_ref_buffer_bytes_);
  free_i16_buffer(&this->prealloc_tx_ref_mono_buffer_, &this->prealloc_tx_ref_mono_buffer_bytes_);
  free_i16_buffer(&this->prealloc_aec_output_, &this->prealloc_aec_output_bytes_);
  free_i16_buffer(&this->prealloc_tx_clock_buffer_, &this->prealloc_tx_clock_buffer_bytes_);
  if (this->mic_alc_handle_ != nullptr) {
    esp_ae_alc_close(static_cast<esp_ae_alc_handle_t>(this->mic_alc_handle_));
    this->mic_alc_handle_ = nullptr;
    this->mic_alc_gain_db_ = 0;
  }
#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
  free_i16_buffer(&this->prealloc_tdm_tx_buffer_, &this->prealloc_tdm_tx_buffer_bytes_);
  free_i16_buffer(&this->prealloc_tx_silence_buffer_, &this->prealloc_tx_silence_buffer_bytes_);
#endif
#ifdef USE_ESP_AUDIO_STACK_STEREO_TX
  free_i16_buffer(&this->prealloc_tx_interleave_buffer_, &this->prealloc_tx_interleave_buffer_bytes_);
#endif
#ifdef USE_ESP_AUDIO_STACK_32BIT
  free_i16_buffer(&this->prealloc_tx_32_buffer_, &this->prealloc_tx_32_buffer_bytes_);
  if (this->tx_bit_cvt_handle_ != nullptr) {
    esp_ae_bit_cvt_close(static_cast<esp_ae_bit_cvt_handle_t>(this->tx_bit_cvt_handle_));
    this->tx_bit_cvt_handle_ = nullptr;
    this->tx_bit_cvt_channels_ = 0;
  }
#endif
#if defined(USE_ESP_AUDIO_STACK_STEREO_TX) && defined(USE_ESP_AUDIO_STACK_MONO_REF)
  if (this->tx_ref_ch_cvt_handle_ != nullptr) {
    esp_ae_ch_cvt_close(static_cast<esp_ae_ch_cvt_handle_t>(this->tx_ref_ch_cvt_handle_));
    this->tx_ref_ch_cvt_handle_ = nullptr;
  }
#endif
#ifdef USE_ESP_AUDIO_STACK_MONO_REF
  free_i16_buffer(&this->direct_aec_ref_);
  this->direct_aec_ref_valid_ = false;
#endif
#ifdef USE_ESP_AUDIO_STACK_RING_REF
  this->aec_ref_ring_buffer_.reset();
#endif

  this->audio_buffers_allocated_ = false;
}

bool ESPAudioStack::allocate_audio_buffers_(AudioTaskCtx &ctx) {
  const uint32_t buf_caps =
      this->buffers_in_psram_ ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  // Worst-case processor mic channels: 2 if dual-mic TDM is available, else 1.
  // Allocating for 2ch unconditionally when dual-mic is possible lets the task
  // flip between MR (1 mic) and MMR (2 mic) without reallocating on
  // reconfigure.
  const uint8_t worst_mic_ch = (this->tdm_second_mic_slot_ >= 0) ? 2 : 1;
  bool processor_buffers_needed = false;
#ifdef USE_AUDIO_PROCESSOR
  // Allocate for the processor once its frame shape is known, even if AFE
  // feed/fetch workers are still parked. This moves heap pressure out of the
  // first media/call activation without starting I2S or esp-sr work.
  processor_buffers_needed = this->processor_ != nullptr && ctx.processor_spec_loaded;
#endif
  const size_t rx_bytes = ctx.rx_frame_bytes;
  const size_t mic_bytes = ctx.mic_separate ? ctx.input_frame_bytes : 0;
  const size_t processor_mic_bytes =
      (processor_buffers_needed && worst_mic_ch > 1) ? ctx.input_frame_bytes * worst_mic_ch : 0;
  const size_t spk_bytes = ctx.speaker_frame_bytes;
  const size_t tx_ref_mono_bytes = (processor_buffers_needed && !ctx.use_tdm_bus && ctx.speaker_channels > 1 &&
                                    !ctx.use_stereo_aec_ref && !ctx.use_tdm_ref)
                                       ? ctx.bus_frame_bytes
                                       : 0;
  const size_t spk_ref_bytes =
      (ctx.use_stereo_aec_ref || ctx.use_tdm_ref || processor_buffers_needed) ? ctx.input_frame_bytes : 0;
  const size_t aec_output_bytes = processor_buffers_needed ? ctx.output_frame_bytes : 0;
  size_t tdm_tx_bytes = 0;
  size_t tx_silence_bytes = 0;
#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
  tdm_tx_bytes = ctx.use_tdm_bus ? ctx.bus_frame_size * ctx.tdm_total_slots * ctx.i2s_bps : 0;
  tx_silence_bytes = ctx.use_tdm_bus ? ctx.bus_frame_bytes : 0;
#endif
  size_t tx_interleave_bytes = 0;
#ifdef USE_ESP_AUDIO_STACK_STEREO_TX
  tx_interleave_bytes = (!ctx.use_tdm_bus && ctx.num_ch > 1 && ctx.speaker_channels == 1)
                            ? ctx.bus_frame_size * ctx.num_ch * sizeof(int16_t)
                            : 0;
#endif
  const size_t tx_clock_bytes = ctx.use_tdm_bus ? tdm_tx_bytes : ctx.bus_frame_size * ctx.num_ch * ctx.i2s_bps;
  size_t tx_32_bytes = 0;
#ifdef USE_ESP_AUDIO_STACK_32BIT
  tx_32_bytes = ctx.i2s_bps == 4 ? (ctx.use_tdm_bus ? tdm_tx_bytes : ctx.bus_frame_size * ctx.num_ch * ctx.i2s_bps) : 0;
#endif

  auto prepare_rate_converters = [&]() -> bool {
    bool ready = true;
    bool rx_prepared = false;
#ifdef USE_ESP_AUDIO_STACK_MULTI_RX
    if (ctx.use_tdm_bus || ctx.use_stereo_aec_ref) {
      ready = this->rx_rate_converter_.prepare(ctx.bus_frame_size, ctx.input_frame_size, ctx.rx_rate_converter_channels,
                                               ctx.use_tdm_bus ? ctx.tdm_total_slots : 2, ctx.i2s_bps == 4);
      rx_prepared = true;
    }
#endif
#ifdef USE_ESP_AUDIO_STACK_MONO_RX
    if (!rx_prepared && (ctx.ratio > 1 || ctx.i2s_bps == 4)) {
      // Mono RX uses esp_audio_effects for both rate and bit-depth conversion.
      ready = this->mic_rate_converter_.prepare(ctx.bus_frame_size, ctx.i2s_bps == 4);
      rx_prepared = true;
    }
#endif
#ifdef USE_ESP_AUDIO_STACK_MONO_REF
    (void) rx_prepared;
    if (ready && processor_buffers_needed && !ctx.use_stereo_aec_ref && !ctx.use_tdm_ref) {
      // No-codec / software-reference AEC converts the TX speaker frame into
      // direct_aec_ref_. Keep that converter hot before speaker playback
      // starts.
      ready = this->play_ref_rate_converter_.prepare(ctx.bus_frame_size);
    }
#if defined(USE_ESP_AUDIO_STACK_STEREO_TX)
    if (ready && tx_ref_mono_bytes > 0 && this->tx_ref_ch_cvt_handle_ == nullptr) {
      esp_ae_ch_cvt_cfg_t cfg{};
      cfg.sample_rate = this->sample_rate_;
      cfg.bits_per_sample = 16;
      cfg.src_ch = ctx.speaker_channels;
      cfg.dest_ch = 1;
      cfg.weight = nullptr;
      cfg.weight_len = 0;
      esp_ae_ch_cvt_handle_t handle = nullptr;
      const esp_ae_err_t err = esp_ae_ch_cvt_open(&cfg, &handle);
      if (err != ESP_AE_ERR_OK || handle == nullptr) {
        ESP_LOGE(TAG, "esp_ae_ch_cvt_open TX ref failed: err=%d ch=%u", static_cast<int>(err),
                 (unsigned) ctx.speaker_channels);
        ready = false;
      } else {
        this->tx_ref_ch_cvt_handle_ = handle;
      }
    }
#endif
#else
    (void) rx_prepared;
#endif
    if (ready && this->mic_alc_handle_ == nullptr) {
      // esp_ae_alc_open allocates internal state. Prepare it with the rest of
      // the audio graph so a later positive mic gain never allocates from the
      // realtime audio loop.
      esp_ae_alc_cfg_t cfg{};
      cfg.sample_rate = this->get_output_sample_rate();
      cfg.channel = 1;
      cfg.bits_per_sample = 16;
      esp_ae_alc_handle_t handle = nullptr;
      const esp_ae_err_t err = esp_ae_alc_open(&cfg, &handle);
      if (err != ESP_AE_ERR_OK || handle == nullptr) {
        ESP_LOGE(TAG, "esp_ae_alc_open mic preallocation failed: err=%d rate=%u", static_cast<int>(err),
                 static_cast<unsigned>(cfg.sample_rate));
        ready = false;
      } else {
        this->mic_alc_handle_ = handle;
        this->mic_alc_gain_db_ = 0;
      }
    }
#ifdef USE_ESP_AUDIO_STACK_32BIT
    if (ready && tx_32_bytes > 0) {
      const uint8_t tx_channels = ctx.use_tdm_bus ? ctx.tdm_total_slots : ctx.num_ch;
      if (this->tx_bit_cvt_handle_ != nullptr && this->tx_bit_cvt_channels_ != tx_channels) {
        esp_ae_bit_cvt_close(static_cast<esp_ae_bit_cvt_handle_t>(this->tx_bit_cvt_handle_));
        this->tx_bit_cvt_handle_ = nullptr;
        this->tx_bit_cvt_channels_ = 0;
      }
      if (this->tx_bit_cvt_handle_ == nullptr) {
        esp_ae_bit_cvt_cfg_t cfg{};
        cfg.sample_rate = this->sample_rate_;
        cfg.channel = tx_channels;
        cfg.src_bits = 16;
        cfg.dest_bits = 32;
        esp_ae_bit_cvt_handle_t handle = nullptr;
        const esp_ae_err_t err = esp_ae_bit_cvt_open(&cfg, &handle);
        if (err != ESP_AE_ERR_OK || handle == nullptr) {
          ESP_LOGE(TAG, "esp_ae_bit_cvt_open TX preallocation failed: err=%d ch=%u", static_cast<int>(err),
                   static_cast<unsigned>(tx_channels));
          ready = false;
        } else {
          this->tx_bit_cvt_handle_ = handle;
          this->tx_bit_cvt_channels_ = tx_channels;
        }
      }
    }
#endif
    return ready;
  };

  if (this->audio_buffers_allocated_) {
    const bool fits =
        this->prealloc_rx_buffer_ != nullptr && this->prealloc_rx_buffer_bytes_ >= rx_bytes &&
        (!ctx.mic_separate ||
         (this->prealloc_mic_buffer_ != nullptr && this->prealloc_mic_buffer_bytes_ >= mic_bytes)) &&
        (processor_mic_bytes == 0 || (this->prealloc_processor_mic_buffer_ != nullptr &&
                                      this->prealloc_processor_mic_buffer_bytes_ >= processor_mic_bytes)) &&
        this->prealloc_spk_buffer_ != nullptr && this->prealloc_spk_buffer_bytes_ >= spk_bytes &&
        (spk_ref_bytes == 0 ||
         (this->prealloc_spk_ref_buffer_ != nullptr && this->prealloc_spk_ref_buffer_bytes_ >= spk_ref_bytes)) &&
        (tx_ref_mono_bytes == 0 || (this->prealloc_tx_ref_mono_buffer_ != nullptr &&
                                    this->prealloc_tx_ref_mono_buffer_bytes_ >= tx_ref_mono_bytes)) &&
        (aec_output_bytes == 0 ||
         (this->prealloc_aec_output_ != nullptr && this->prealloc_aec_output_bytes_ >= aec_output_bytes))
#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
        && (tdm_tx_bytes == 0 ||
            (this->prealloc_tdm_tx_buffer_ != nullptr && this->prealloc_tdm_tx_buffer_bytes_ >= tdm_tx_bytes)) &&
        (tx_silence_bytes == 0 ||
         (this->prealloc_tx_silence_buffer_ != nullptr && this->prealloc_tx_silence_buffer_bytes_ >= tx_silence_bytes))
#endif
#ifdef USE_ESP_AUDIO_STACK_STEREO_TX
        && (tx_interleave_bytes == 0 || (this->prealloc_tx_interleave_buffer_ != nullptr &&
                                         this->prealloc_tx_interleave_buffer_bytes_ >= tx_interleave_bytes))
#endif
        && (this->prealloc_tx_clock_buffer_ != nullptr && this->prealloc_tx_clock_buffer_bytes_ >= tx_clock_bytes)
#ifdef USE_ESP_AUDIO_STACK_32BIT
        && (tx_32_bytes == 0 ||
            (this->prealloc_tx_32_buffer_ != nullptr && this->prealloc_tx_32_buffer_bytes_ >= tx_32_bytes))
#endif
        ;
    if (fits) {
      if (!prepare_rate_converters()) {
        this->release_audio_buffers_();
        return false;
      }
      return true;
    }
    ESP_LOGI(TAG, "Audio buffer shape changed; reallocating buffers");
    this->release_audio_buffers_();
  }

  alloc_i16_buffer(&this->prealloc_rx_buffer_, &this->prealloc_rx_buffer_bytes_, rx_bytes, buf_caps, false);

  if (ctx.mic_separate) {
    alloc_i16_buffer(&this->prealloc_mic_buffer_, &this->prealloc_mic_buffer_bytes_, mic_bytes, buf_caps, true);
  }

  if (processor_mic_bytes > 0) {
    alloc_i16_buffer(&this->prealloc_processor_mic_buffer_, &this->prealloc_processor_mic_buffer_bytes_,
                     processor_mic_bytes, buf_caps, true);
  }

  alloc_i16_buffer(&this->prealloc_spk_buffer_, &this->prealloc_spk_buffer_bytes_, spk_bytes, buf_caps, false);

  if (ctx.use_stereo_aec_ref || ctx.use_tdm_ref) {
    alloc_i16_buffer(&this->prealloc_spk_ref_buffer_, &this->prealloc_spk_ref_buffer_bytes_, spk_ref_bytes, buf_caps,
                     true);
  }

#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
  if (ctx.use_tdm_bus) {
    alloc_i16_buffer(&this->prealloc_tdm_tx_buffer_, &this->prealloc_tdm_tx_buffer_bytes_, tdm_tx_bytes, buf_caps,
                     false);
    alloc_i16_buffer(&this->prealloc_tx_silence_buffer_, &this->prealloc_tx_silence_buffer_bytes_, tx_silence_bytes,
                     buf_caps, true);
    if (this->prealloc_tx_silence_buffer_ != nullptr) {
      memset(this->prealloc_tx_silence_buffer_, 0, tx_silence_bytes);
    }
  }
#endif

  if (tx_ref_mono_bytes > 0) {
    alloc_i16_buffer(&this->prealloc_tx_ref_mono_buffer_, &this->prealloc_tx_ref_mono_buffer_bytes_, tx_ref_mono_bytes,
                     buf_caps, true);
  }

#ifdef USE_ESP_AUDIO_STACK_STEREO_TX
  if (tx_interleave_bytes > 0) {
    alloc_i16_buffer(&this->prealloc_tx_interleave_buffer_, &this->prealloc_tx_interleave_buffer_bytes_,
                     tx_interleave_bytes, buf_caps, true);
  }
#endif

  alloc_i16_buffer(&this->prealloc_tx_clock_buffer_, &this->prealloc_tx_clock_buffer_bytes_, tx_clock_bytes, buf_caps,
                   false);
  if (this->prealloc_tx_clock_buffer_ != nullptr) {
    memset(this->prealloc_tx_clock_buffer_, 0, tx_clock_bytes);
  }

#ifdef USE_ESP_AUDIO_STACK_32BIT
  if (tx_32_bytes > 0) {
    alloc_i16_buffer(&this->prealloc_tx_32_buffer_, &this->prealloc_tx_32_buffer_bytes_, tx_32_bytes, buf_caps, false);
  }
#endif

#ifdef USE_AUDIO_PROCESSOR
  if (processor_buffers_needed) {
    if (!this->prealloc_spk_ref_buffer_ && !ctx.use_tdm_ref) {
      alloc_i16_buffer(&this->prealloc_spk_ref_buffer_, &this->prealloc_spk_ref_buffer_bytes_, spk_ref_bytes, buf_caps,
                       true);
    }
    alloc_i16_buffer(&this->prealloc_aec_output_, &this->prealloc_aec_output_bytes_, aec_output_bytes, buf_caps, true);

#ifdef USE_ESP_AUDIO_STACK_MONO_REF
    // direct_aec_ref_ stores the converted TX reference at the processor rate.
    // Sized to ctx.input_frame_bytes: the AEC reference is the signal that
    // enters the DSP alongside the mic, so it lives on the input side of the
    // processor (AudioProcessor::process expects in_ref of input_samples len).
    // The TX-side rate converter writes input_frame_size samples here per
    // frame. Honours buffers_in_psram_ alongside the rest.
    if (!this->direct_aec_ref_ && !ctx.use_stereo_aec_ref && !ctx.use_tdm_ref) {
      alloc_i16_buffer(&this->direct_aec_ref_, nullptr, ctx.input_frame_bytes, buf_caps, true);
      if (this->direct_aec_ref_ != nullptr) {
        memset(this->direct_aec_ref_, 0, ctx.input_frame_bytes);
      }
    }

    // AEC reference ring buffer (Espressif/ADF TYPE2-style, no-codec setups).
#ifdef USE_ESP_AUDIO_STACK_RING_REF
    if (!ctx.use_stereo_aec_ref && !ctx.use_tdm_ref && !this->aec_ref_ring_buffer_) {
      // Sized at the processor rate (post-rate-conversion), not the bus rate,
      // since we now convert on the TX side before storing. Items pushed are
      // input_frame_bytes (one AEC reference frame) each.
      const uint32_t output_rate = this->sample_rate_ / ctx.ratio;
      size_t rb_bytes = (output_rate * this->aec_ref_buffer_ms_ / 1000) * sizeof(int16_t);
      if (rb_bytes < ctx.input_frame_bytes * 4)
        rb_bytes = ctx.input_frame_bytes * 4;
      // Placement YAML-controlled: internal saves ~13.6 us/frame on Core 0 (R+W
      // ~1 KB each), PSRAM saves ~3-5 KB internal RAM (set
      // aec_ref_ring_in_psram).
      this->aec_ref_ring_buffer_ = this->aec_ref_ring_in_psram_ ? create_prefer_psram(rb_bytes, "audio_stack.aec_ref")
                                                                : create_internal(rb_bytes, "audio_stack.aec_ref");
      if (!this->aec_ref_ring_buffer_) {
        this->release_audio_buffers_();
        return false;
      }
      ESP_LOGI(TAG, "AEC reference: ring_buffer (%zu bytes, %ums capacity)", rb_bytes,
               (unsigned) this->aec_ref_buffer_ms_);
    }
#endif
#ifdef USE_ESP_AUDIO_STACK_PREVIOUS_FRAME_REF
    if (!ctx.use_stereo_aec_ref && !ctx.use_tdm_ref) {
      ESP_LOGI(TAG, "AEC reference: previous_frame");
    }
#endif
#endif
  }
#endif

  // Validate required allocations
  if (!this->prealloc_rx_buffer_ || !this->prealloc_spk_buffer_) {
    this->release_audio_buffers_();
    return false;
  }
  if (ctx.mic_separate && !this->prealloc_mic_buffer_) {
    this->release_audio_buffers_();
    return false;
  }
  if (processor_mic_bytes > 0 && !this->prealloc_processor_mic_buffer_) {
    this->release_audio_buffers_();
    return false;
  }
#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
  if (ctx.use_tdm_bus && !this->prealloc_tdm_tx_buffer_) {
    this->release_audio_buffers_();
    return false;
  }
  if (ctx.use_tdm_bus && !this->prealloc_tx_silence_buffer_) {
    this->release_audio_buffers_();
    return false;
  }
#endif
  if (tx_ref_mono_bytes > 0 && !this->prealloc_tx_ref_mono_buffer_) {
    this->release_audio_buffers_();
    return false;
  }
#ifdef USE_ESP_AUDIO_STACK_STEREO_TX
  if (tx_interleave_bytes > 0 && !this->prealloc_tx_interleave_buffer_) {
    this->release_audio_buffers_();
    return false;
  }
#endif
#ifdef USE_ESP_AUDIO_STACK_32BIT
  if (tx_32_bytes > 0 && !this->prealloc_tx_32_buffer_) {
    this->release_audio_buffers_();
    return false;
  }
#endif
#ifdef USE_AUDIO_PROCESSOR
  if (processor_buffers_needed) {
    if (!this->prealloc_aec_output_) {
      this->release_audio_buffers_();
      return false;
    }
    if ((ctx.use_stereo_aec_ref || ctx.use_tdm_ref) && !this->prealloc_spk_ref_buffer_) {
      this->release_audio_buffers_();
      return false;
    }
#ifdef USE_ESP_AUDIO_STACK_MONO_REF
    // Mono AEC depends on direct_aec_ref_ as both the TX-side rate-conversion
    // scratch and the previous-frame store. If it failed to allocate, the AEC
    // would silently run with a zero reference (TX writer is null-gated, ring
    // writer too, fill_mono falls through to zero-fill) and stay degraded until
    // reboot, because audio_buffers_allocated_ latches true on success.
    // Fail-closed.
    if (!ctx.use_stereo_aec_ref && !ctx.use_tdm_ref && !this->direct_aec_ref_) {
      ESP_LOGE(TAG, "Mono AEC reference buffer allocation failed (%zu bytes)", ctx.input_frame_bytes);
      this->release_audio_buffers_();
      return false;
    }
    if (!ctx.use_tdm_bus && ctx.speaker_channels > 1 && !ctx.use_stereo_aec_ref && !ctx.use_tdm_ref &&
        !this->prealloc_tx_ref_mono_buffer_) {
      ESP_LOGE(TAG, "Stereo TX mono reference buffer allocation failed (%zu bytes)", ctx.bus_frame_bytes);
      this->release_audio_buffers_();
      return false;
    }
#endif
  }
#endif

  if (!prepare_rate_converters()) {
    this->release_audio_buffers_();
    return false;
  }

  this->audio_buffers_allocated_ = true;
  return true;
}

void ESPAudioStack::audio_task(void *param) {
  ESPAudioStack *self = static_cast<ESPAudioStack *>(param);
  self->audio_task_();
  vTaskDelete(nullptr);
}

void ESPAudioStack::audio_task_() {
  // Component lifetime is process-wide (no destructor in ESPHome), so
  // this task lives forever; stop()/start() just toggle audio_stack_running_
  // to park/wake the inner session loop.
  while (true) {
    this->audio_task_idle_.store(true, std::memory_order_relaxed);
    TaskHandle_t waiter = this->audio_state_waiter_.load(std::memory_order_acquire);
    if (waiter != nullptr) {
      xTaskNotifyGive(waiter);
    }
    if (this->prealloc_requested_.exchange(false, std::memory_order_acq_rel)) {
      this->preallocate_audio_buffers_from_task_();
    }
    this->service_speaker_reset_();
    if (!this->audio_stack_running_.load(std::memory_order_relaxed)) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      continue;
    }
    this->audio_task_idle_.store(false, std::memory_order_relaxed);
    waiter = this->audio_state_waiter_.load(std::memory_order_acquire);
    if (waiter != nullptr) {
      xTaskNotifyGive(waiter);
    }

    // Run one audio session. Returns on stop() (audio_stack_running_ cleared)
    // or on processor frame_spec change (session restarts in the next outer
    // iter).
    this->audio_session_();
  }
}

bool ESPAudioStack::prepare_audio_context_(AudioTaskCtx &ctx, bool require_processor_spec, bool log_context) {
  ctx.ratio = this->rate_conversion_ratio_;
  ctx.i2s_bps = (this->bits_per_sample_ > 16) ? 4 : 2;
  ctx.num_ch = this->num_channels_;
  ctx.speaker_channels = this->get_speaker_channels();
  ctx.use_stereo_aec_ref = this->use_stereo_aec_ref_;
  ctx.rx_slot_mode_stereo = this->rx_slot_mode_stereo_;
  ctx.use_tdm_bus = this->use_tdm_bus_;
  ctx.use_tdm_ref = this->use_tdm_ref_;
  ctx.ref_channel_right = this->ref_channel_right_;
  ctx.correct_dc_offset = this->correct_dc_offset_;
  ctx.tdm_total_slots = this->tdm_total_slots_;
  ctx.tdm_mic_slot = this->tdm_mic_slot_;
  ctx.tdm_second_mic_slot = this->tdm_second_mic_slot_;
  ctx.tdm_ref_slot = this->tdm_ref_slot_;
  ctx.tdm_tx_slot = this->tdm_tx_slot_;

  if (log_context) {
    ESP_LOGI(TAG, "Audio task started (stereo=%s, tdm=%s, rate_conversion=%ux)", ctx.use_stereo_aec_ref ? "YES" : "no",
             ctx.use_tdm_bus ? (ctx.use_tdm_ref ? "YES/ref" : "YES/mic") : "no", (unsigned) ctx.ratio);
  }

  // Determine frame sizes: processors may consume and produce different frame
  // lengths.
  ctx.input_frame_size = DEFAULT_FRAME_SIZE;
  ctx.output_frame_size = DEFAULT_FRAME_SIZE;
#ifdef USE_AUDIO_PROCESSOR
  if (this->processor_ != nullptr) {
    ctx.processor_spec_revision = this->processor_->frame_spec_revision();
    auto spec = this->processor_->frame_spec();
    if (spec.input_samples > 0 && spec.output_samples > 0) {
      ctx.input_frame_size = spec.input_samples;
      ctx.output_frame_size = spec.output_samples;
      ctx.processor_mic_channels = std::max<uint8_t>(1, spec.mic_channels);
      ctx.processor_spec_loaded = true;
      uint32_t out_rate = this->get_output_sample_rate();
      if (log_context) {
        ESP_LOGI(TAG,
                 "Processor: input=%u, output=%u samples, mic_ch=%u (%ums @ "
                 "%uHz), revision=%u",
                 (unsigned) ctx.input_frame_size, (unsigned) ctx.output_frame_size,
                 (unsigned) ctx.processor_mic_channels, (unsigned) (ctx.input_frame_size * 1000 / out_rate),
                 (unsigned) out_rate, (unsigned) ctx.processor_spec_revision);
      }
    } else if (require_processor_spec) {
      return false;
    }
  }
#endif

#ifdef USE_ESP_AUDIO_STACK_MULTI_RX
  // Init multi-channel RX rate converter now that we know channel count
  if (ctx.use_tdm_bus || ctx.use_stereo_aec_ref || ctx.rx_slot_mode_stereo) {
    ctx.rx_rate_converter_channels =
        ctx.use_tdm_bus ? (ctx.processor_mic_channels > 1 ? 2 : 1) + (ctx.use_tdm_ref ? 1 : 0)
                        : (ctx.use_stereo_aec_ref ? 2 : 1);  // stereo ref: mic + ref; stereo mic: selected mic only
    this->rx_rate_converter_.init(ctx.ratio, ctx.rx_rate_converter_channels, this->sample_rate_,
                                  this->get_output_sample_rate(), this->rate_cvt_complexity_,
                                  this->rate_cvt_perf_type_);
  }
#endif

  // ── Frame sizing ──
  ctx.bus_frame_size = ctx.input_frame_size * ctx.ratio;
  ctx.input_frame_bytes = ctx.input_frame_size * sizeof(int16_t);
  ctx.output_frame_bytes = ctx.output_frame_size * sizeof(int16_t);
  ctx.bus_frame_bytes = ctx.bus_frame_size * sizeof(int16_t);
  ctx.speaker_frame_bytes = ctx.bus_frame_bytes * ctx.speaker_channels;
  if (ctx.use_tdm_bus) {
    ctx.rx_frame_bytes = ctx.bus_frame_size * ctx.tdm_total_slots * ctx.i2s_bps;
  } else if (ctx.use_stereo_aec_ref || ctx.rx_slot_mode_stereo) {
    ctx.rx_frame_bytes = ctx.bus_frame_size * 2 * ctx.i2s_bps;
  } else {
    ctx.rx_frame_bytes = ctx.bus_frame_size * ctx.i2s_bps;
  }
  ctx.mic_separate =
      (ctx.ratio > 1) || ctx.use_stereo_aec_ref || ctx.rx_slot_mode_stereo || ctx.use_tdm_bus || (ctx.i2s_bps == 4);
  if (ctx.use_tdm_bus) {
    ctx.rx_path_kind = RxPathKind::TDM;
  } else if (ctx.use_stereo_aec_ref) {
    ctx.rx_path_kind = RxPathKind::STEREO_REF;
  } else if (ctx.rx_slot_mode_stereo) {
    ctx.rx_path_kind = RxPathKind::STEREO_SLOT;
  } else if (ctx.ratio > 1 || ctx.i2s_bps == 4) {
    ctx.rx_path_kind = RxPathKind::MONO_EFFECTS;
  } else {
    ctx.rx_path_kind = RxPathKind::PASSTHROUGH;
  }
  return true;
}

void ESPAudioStack::preallocate_audio_buffers_from_task_() {
  if (this->audio_buffers_allocated_) {
    this->prealloc_attempted_.store(true, std::memory_order_release);
    return;
  }
  if (this->prealloc_attempted_.exchange(true, std::memory_order_acq_rel)) {
    return;
  }

  AudioTaskCtx ctx{};
  if (!this->prepare_audio_context_(ctx, true, false)) {
    this->prealloc_attempted_.store(false, std::memory_order_release);
    return;
  }
  if (this->allocate_audio_buffers_(ctx)) {
    ESP_LOGI(TAG, "Audio buffers preallocated (rx=%uB, spk=%uB, processor=%s)",
             (unsigned) this->prealloc_rx_buffer_bytes_, (unsigned) this->prealloc_spk_buffer_bytes_,
             ctx.processor_spec_loaded ? "yes" : "no");
    this->log_memory_snapshot_("after_audio_buffer_prealloc");
  } else {
    ESP_LOGE(TAG, "Audio buffer preallocation failed; refusing cold-path allocation");
    this->has_i2s_error_.store(true, std::memory_order_relaxed);
  }
}

void ESPAudioStack::audio_session_() {
  AudioTaskCtx ctx{};
  // Telemetry compute paths gated on log level: when ESPHOME_LOG_LEVEL is
  // below DEBUG the entire block is stripped, leaving zero runtime cost
  // (no per-frame counters, no processor_->telemetry() call, no heap probes).
#if defined(USE_ESP_AUDIO_STACK_TELEMETRY) && ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
  uint32_t t_frame_count = 0;
  uint32_t t_spk_underruns = 0;
  int64_t t_previous_frame_start_us = 0;
  uint64_t t_frame_interval_sum_us = 0;
  uint32_t t_frame_interval_max_us = 0;
  uint32_t t_frame_interval_samples = 0;
#ifdef USE_AUDIO_PROCESSOR
  ProcessorTelemetry prev_processor_telem{};
#endif
#endif

  // ── Populate invariants and frame sizing ──
  if (!this->prepare_audio_context_(ctx, false, true)) {
    this->has_i2s_error_.store(true, std::memory_order_relaxed);
    this->audio_stack_running_.store(false, std::memory_order_relaxed);
    this->speaker_running_.store(false, std::memory_order_relaxed);
    return;
  }

  // ── Buffer setup ──
  // Working buffers are owned by the component and pre-allocated worst-case
  // on first task entry (see allocate_audio_buffers_). Subsequent restarts
  // (frame_spec change, feature toggle) reuse the same pointers without any
  // heap_caps_alloc calls, eliminating SPIRAM fragmentation that previously
  // caused "Failed to allocate AEC output buffer" after a few reconfigures.

  auto alloc_fail = [this](const char *what) {
    ESP_LOGE(TAG, "Failed to allocate %s", what);
    this->has_i2s_error_.store(true, std::memory_order_relaxed);
    this->audio_stack_running_.store(false, std::memory_order_relaxed);
    this->speaker_running_.store(false, std::memory_order_relaxed);
  };

  if (ctx.processor_mic_channels > 2) {
    alloc_fail("unsupported processor mic channel count");
    ESP_LOGD(TAG, "Audio session ended");
    return;
  }
  if (ctx.processor_mic_channels > 1 && !ctx.use_tdm_bus) {
    alloc_fail("dual-mic processor requires TDM microphone slots");
    ESP_LOGD(TAG, "Audio session ended");
    return;
  }
  if (ctx.processor_mic_channels > 1 && ctx.tdm_second_mic_slot < 0) {
    alloc_fail("dual-mic processor requires tdm_mic_slots with two slots");
    ESP_LOGD(TAG, "Audio session ended");
    return;
  }

  if (!this->audio_buffers_allocated_) {
    alloc_fail("audio buffers (not preallocated)");
    ESP_LOGD(TAG, "Audio session ended");
    return;
  }
  if (!this->allocate_audio_buffers_(ctx)) {
    alloc_fail("audio buffers (prepared shape invalid)");
    ESP_LOGD(TAG, "Audio session ended");
    return;
  }

  ctx.rx_buffer = this->prealloc_rx_buffer_;
  ctx.mic_buffer = ctx.mic_separate ? this->prealloc_mic_buffer_ : ctx.rx_buffer;
  if (ctx.processor_mic_channels > 1) {
    ctx.processor_mic_buffer = this->prealloc_processor_mic_buffer_;
  }
  ctx.spk_buffer = this->prealloc_spk_buffer_;
  ctx.spk_ref_buffer = this->prealloc_spk_ref_buffer_;
  ctx.tx_ref_mono_buffer = this->prealloc_tx_ref_mono_buffer_;
#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
  ctx.tdm_tx_buffer = this->prealloc_tdm_tx_buffer_;
  ctx.tx_silence_buffer = this->prealloc_tx_silence_buffer_;
#endif
#ifdef USE_ESP_AUDIO_STACK_STEREO_TX
  ctx.tx_interleave_buffer = this->prealloc_tx_interleave_buffer_;
#endif
#ifdef USE_ESP_AUDIO_STACK_32BIT
  ctx.tx_32_buffer = this->prealloc_tx_32_buffer_;
#endif
  ctx.aec_output = this->prealloc_aec_output_;
  ctx.tx_clock_buffer = this->prealloc_tx_clock_buffer_;
#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
  if (ctx.use_tdm_bus) {
    ctx.tdm_tx_frame_bytes = ctx.bus_frame_size * ctx.tdm_total_slots * ctx.i2s_bps;
  }
#endif

#if defined(USE_ESP_AUDIO_STACK_TELEMETRY) && ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
  ESP_LOGD(TAG, "Heap after audio init: internal=%u, PSRAM=%u", (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#endif

  // ── Main loop ──
  while (this->audio_stack_running_.load(std::memory_order_relaxed)) {
#if defined(USE_ESP_AUDIO_STACK_TELEMETRY) && ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
    const int64_t frame_start_us = esp_timer_get_time();
    if (t_previous_frame_start_us != 0) {
      const uint32_t interval_us =
          static_cast<uint32_t>(std::max<int64_t>(0, frame_start_us - t_previous_frame_start_us));
      t_frame_interval_sum_us += interval_us;
      t_frame_interval_max_us = std::max(t_frame_interval_max_us, interval_us);
      t_frame_interval_samples++;
    }
    t_previous_frame_start_us = frame_start_us;
#endif
    // Service ring buffer operations requested by main thread
    this->service_speaker_reset_();
    this->drain_tx_completion_events_();
    // Reset per-frame state
    ctx.output_buffer = nullptr;
    ctx.current_output_frame_size = ctx.input_frame_size;
    ctx.current_output_frame_bytes = ctx.input_frame_bytes;
    ctx.speaker_underrun = false;
#ifdef USE_ESP_AUDIO_STACK_TDM_REF_DIAGNOSTIC
    ctx.previous_speaker_got = ctx.speaker_got;
    ctx.previous_speaker_dbfs = ctx.current_speaker_dbfs;
    ctx.current_speaker_dbfs = -120.0f;
#endif
    ctx.speaker_got = 0;

    // Snapshot atomic state for this frame (avoids repeated .load() in sample
    // loops)
    ctx.input_gain_q31 = this->input_gain_q31_.load(std::memory_order_relaxed);
    ctx.input_gain_boost = this->input_gain_boost_.load(std::memory_order_relaxed);
    ctx.mic_gain_q31 = this->mic_gain_q31_.load(std::memory_order_relaxed);
    ctx.mic_gain_boost_db = this->mic_gain_boost_db_.load(std::memory_order_relaxed);
    ctx.hot_output_volume_q31 = this->hot_output_volume_q31_.load(std::memory_order_relaxed);
    ctx.speaker_running = this->speaker_running_.load(std::memory_order_relaxed);
    ctx.speaker_paused = this->speaker_paused_.load(std::memory_order_relaxed);
    ctx.mic_running = this->has_mic_consumers_.load(std::memory_order_relaxed);
#ifdef USE_ESP_AUDIO_STACK_RING_REF
    // Never carry playback history into a new capture session. In TYPE2-style
    // software-reference mode the speaker may run long before a mic consumer
    // appears; retaining that FIFO backlog permanently misaligns AEC because
    // producer and consumer subsequently advance at the same rate.
    if (this->aec_ref_ring_buffer_ && ctx.mic_running != ctx.aec_ref_mic_active) {
      this->aec_ref_ring_buffer_->reset();
      ctx.aec_ref_mic_active = ctx.mic_running;
    }
#endif
    this->update_runtime_audio_flags_(ctx);

    ctx.processor_enabled = this->processor_enabled_.load(std::memory_order_relaxed);
#ifdef USE_AUDIO_PROCESSOR
    ctx.processor_ready = ctx.processor_enabled && this->processor_ != nullptr && this->processor_->is_initialized();
#endif
    ctx.now_ms = millis();

    if (ctx.need_rx_processing) {
      this->process_rx_path_(ctx);
    } else if (ctx.need_rx_drain) {
      this->drain_rx_minimal_(ctx);
    }

    if (ctx.need_rx_processing) {
      this->process_aec_and_callbacks_(ctx);
    }
    if (ctx.clock_only_tx) {
      this->process_tx_clock_only_(ctx);
    } else if (ctx.need_tx_audio) {
      this->process_tx_path_(ctx);
    }

#ifdef USE_AUDIO_PROCESSOR
    // Frame_spec change, for example an AFE mode or graph rebuild: exit this
    // session so the outer audio_task_ wrapper can re-enter audio_session_ with
    // a fresh ctx. Preallocated buffers are already worst-case sized, so the
    // restart does not touch the heap.
    if (this->processor_ != nullptr) {
      uint32_t rev = this->processor_->frame_spec_revision();
      bool ready = this->processor_->is_initialized();
      if (ready && !ctx.processor_spec_loaded) {
        ESP_LOGI(TAG,
                 "Processor frame_spec became available (rev %u), restarting "
                 "session",
                 (unsigned) rev);
        break;
      }
      if (ready && rev != ctx.processor_spec_revision) {
        ESP_LOGI(TAG, "Processor frame_spec changed (rev %u -> %u), restarting session",
                 (unsigned) ctx.processor_spec_revision, (unsigned) rev);
        break;
      }
      if (!ready) {
        ctx.processor_spec_revision = rev;
      }
    }
#endif

#if defined(USE_ESP_AUDIO_STACK_TELEMETRY) && ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
    {
      // Lightweight per-frame cycle snapshot (only when telemetry: true AND
      // log level >= DEBUG: otherwise this block is stripped at compile time
      // so the loop pays zero runtime cost in production builds).
      // t_frame_count and t_spk_underruns declared before the loop (reset on
      // task restart)
      t_spk_underruns += ctx.speaker_underrun ? 1 : 0;
      t_frame_count++;
      if (t_frame_count >= this->telemetry_log_interval_frames_) {
        const uint32_t frame_interval_avg_us =
            t_frame_interval_samples == 0 ? 0
                                          : static_cast<uint32_t>(t_frame_interval_sum_us / t_frame_interval_samples);
        ESP_LOGD(TAG,
                 "Perf[%u frames]: frame_us avg/max=%u/%u spk_underrun=%u "
                 "heap_int=%u heap_ps=%u",
                 (unsigned) t_frame_count, (unsigned) frame_interval_avg_us, (unsigned) t_frame_interval_max_us,
                 (unsigned) t_spk_underruns, (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#ifdef USE_AUDIO_PROCESSOR
        if (this->processor_ != nullptr) {
          ProcessorTelemetry telem = this->processor_->telemetry();
          const uint32_t audio_stack_high_water = uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t);
          auto delta_u32 = [](uint32_t current, uint32_t previous) -> uint32_t {
            return current >= previous ? (current - previous) : current;
          };
          ESP_LOGD(
              TAG,
              "AFE[%u]: +glitch=%u +in_drop=%u +feed_ok=%u +feed_rej=%u "
              "+fetch_ok=%u +fetch_to=%u +out_drop=%u rb=%.0f%% "
              "q(feed=%u pk=%u, fetch=%u pk=%u)",
              (unsigned) telem.frame_count, (unsigned) delta_u32(telem.glitch_count, prev_processor_telem.glitch_count),
              (unsigned) delta_u32(telem.input_ring_drop, prev_processor_telem.input_ring_drop),
              (unsigned) delta_u32(telem.feed_ok, prev_processor_telem.feed_ok),
              (unsigned) delta_u32(telem.feed_rejected, prev_processor_telem.feed_rejected),
              (unsigned) delta_u32(telem.fetch_ok, prev_processor_telem.fetch_ok),
              (unsigned) delta_u32(telem.fetch_timeout, prev_processor_telem.fetch_timeout),
              (unsigned) delta_u32(telem.output_ring_drop, prev_processor_telem.output_ring_drop),
              telem.ringbuf_free_pct * 100.0f, (unsigned) telem.feed_queue_frames, (unsigned) telem.feed_queue_peak,
              (unsigned) telem.fetch_queue_frames, (unsigned) telem.fetch_queue_peak);
          ESP_LOGD(TAG,
                   "AFE timing: proc last/max=%u/%uus feed last/max=%u/%uus "
                   "fetch last/max=%u/%uus stackB audio=%u",
                   (unsigned) telem.process_us_last, (unsigned) telem.process_us_max, (unsigned) telem.feed_us_last,
                   (unsigned) telem.feed_us_max, (unsigned) telem.fetch_us_last, (unsigned) telem.fetch_us_max,
                   (unsigned) audio_stack_high_water);
          prev_processor_telem = telem;
        }
#endif
        t_frame_count = 0;
        t_spk_underruns = 0;
        t_frame_interval_sum_us = 0;
        t_frame_interval_max_us = 0;
        t_frame_interval_samples = 0;
      }
    }
#endif

    // RX/TX cadence is owned by the blocking codec/I2S DMA operations above.
    // Do not append a tick-based delay: it compounds frame latency (especially
    // at a 100 Hz FreeRTOS tick) and vTaskDelay is not a cross-core clock.
  }

  // Buffers live on as component-owned preallocations; nothing to free here.
  // Returning from audio_session_ hands control back to the outer wrapper,
  // which either parks the task (stop) or re-enters with fresh ctx
  // (frame_spec change).
  ESP_LOGD(TAG, "Audio session ended");
}

void ESPAudioStack::update_runtime_audio_flags_(AudioTaskCtx &ctx) {
  ctx.any_tdm_slot_level_sensor_enabled = this->any_tdm_slot_level_sensor_enabled_.load(std::memory_order_relaxed);
  ctx.need_rx_processing = this->rx_handle_ != nullptr && (ctx.mic_running || ctx.any_tdm_slot_level_sensor_enabled);
  // If RX is enabled but nobody needs the decoded mic surface, still drain the
  // driver/codec queue so DMA does not build backpressure while speaker-only
  // playback is active.
  ctx.need_rx_drain = this->rx_handle_ != nullptr && !ctx.need_rx_processing;
  ctx.need_tx_audio = this->tx_handle_ != nullptr && ctx.speaker_running && !ctx.speaker_paused;
  // ESP-IDF full-duplex shares clock signals; keep TX moving whenever the
  // stack owns TX, even if the frame is just silence for RX clocking.
  ctx.need_tx_clock = this->tx_handle_ != nullptr;
  ctx.clock_only_tx = ctx.need_tx_clock && !ctx.need_tx_audio;
}

void ESPAudioStack::drain_rx_minimal_(AudioTaskCtx &ctx) {
  if (!this->read_rx_frame_(ctx, "drain"))
    return;
}

bool ESPAudioStack::read_rx_frame_(AudioTaskCtx &ctx, const char *label) {
  if (!this->rx_handle_ || ctx.rx_buffer == nullptr)
    return false;

#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  const bool read_ok = this->codec_backend_.read(ctx.rx_buffer, ctx.rx_frame_bytes);
  const bool io_closed = !this->codec_backend_.is_open();
#else
  size_t bytes_read = 0;
  const esp_err_t read_err =
      i2s_channel_read(this->rx_handle_, ctx.rx_buffer, ctx.rx_frame_bytes, &bytes_read, portMAX_DELAY);
  const bool read_ok = read_err == ESP_OK && bytes_read == ctx.rx_frame_bytes;
  const bool io_closed = read_err == ESP_ERR_INVALID_STATE;
#endif
  if (!read_ok && io_closed) {
    if (this->audio_stack_running_.load(std::memory_order_relaxed)) {
      if (++ctx.invalid_state_errors > 100) {
        ESP_LOGE(TAG,
                 "Persistent I2S RX INVALID_STATE during %s (%d) - channel "
                 "corrupted",
                 label, ctx.invalid_state_errors);
        this->has_i2s_error_.store(true, std::memory_order_relaxed);
        this->audio_stack_running_.store(false, std::memory_order_relaxed);
      }
    }
    return false;
  }
  if (!read_ok) {
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
    LOG_W_THROTTLED("Codec RX %s read failed", label);
#else
    LOG_W_THROTTLED("I2S %s read failed: err=%s bytes=%u/%u", label, esp_err_to_name(read_err),
                    static_cast<unsigned>(bytes_read), static_cast<unsigned>(ctx.rx_frame_bytes));
#endif
    if (++ctx.consecutive_i2s_errors > 100) {
      ESP_LOGE(TAG, "Persistent I2S %s read errors (%d)", label, ctx.consecutive_i2s_errors);
      this->has_i2s_error_.store(true, std::memory_order_relaxed);
      this->audio_stack_running_.store(false, std::memory_order_relaxed);
    }
    return false;
  }
  ctx.consecutive_i2s_errors = 0;
  ctx.invalid_state_errors = 0;
  return true;
}

void ESPAudioStack::fail_audio_session_(const char *stage) {
  ESP_LOGE(TAG, "%s failed; stopping audio session", stage);
  this->has_i2s_error_.store(true, std::memory_order_relaxed);
  this->audio_stack_running_.store(false, std::memory_order_relaxed);
}

// ════════════════════════════════════════════════════════════════════════════
// RX PATH: I2S read -> deinterleave/rate-convert -> mic_buffer + spk_ref_buffer
// ════════════════════════════════════════════════════════════════════════════
void ESPAudioStack::process_rx_path_(AudioTaskCtx &ctx) {
  if (!this->read_rx_frame_(ctx, "audio"))
    return;

#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
  if (ctx.use_tdm_bus && ctx.any_tdm_slot_level_sensor_enabled) {
    this->update_tdm_slot_levels_(ctx);
  }
#endif

  ctx.output_buffer = ctx.mic_buffer;  // Default: no AEC processing
  ctx.processor_input = ctx.mic_buffer;
  if (!this->convert_rx_frame_(ctx)) {
    return;
  }
  this->apply_input_conditioning_(ctx);
}

bool ESPAudioStack::convert_rx_frame_(AudioTaskCtx &ctx) {
  switch (ctx.rx_path_kind) {
    case RxPathKind::PASSTHROUGH:
      return this->process_rx_passthrough_(ctx);
    case RxPathKind::MONO_EFFECTS:
      return this->process_rx_mono_effects_(ctx);
    case RxPathKind::STEREO_SLOT:
      return this->process_rx_stereo_slot_(ctx);
    case RxPathKind::STEREO_REF:
      return this->process_rx_stereo_ref_(ctx);
    case RxPathKind::TDM:
      return this->process_rx_tdm_(ctx);
  }
  return false;
}

bool ESPAudioStack::process_rx_passthrough_(AudioTaskCtx &ctx) {
  (void) ctx;
  return true;
}

#if SOC_I2S_SUPPORTS_TDM && defined(USE_ESP_AUDIO_STACK_TDM_BUS)
bool ESPAudioStack::process_rx_tdm_(AudioTaskCtx &ctx) {
  const uint8_t ts = ctx.tdm_total_slots;
  const bool dual_mic = ctx.processor_mic_channels > 1 && ctx.tdm_second_mic_slot >= 0;
  uint8_t ch_offsets[MAX_RATE_CVT_CHANNELS];
  uint8_t num_mic_ch = dual_mic ? 2 : 1;
  uint8_t selected_channels = 0;
  if (dual_mic) {
    ch_offsets[selected_channels++] = ctx.tdm_mic_slot;
    ch_offsets[selected_channels++] = static_cast<uint8_t>(ctx.tdm_second_mic_slot);
  } else {
    ch_offsets[selected_channels++] = ctx.tdm_mic_slot;
  }
  if (ctx.use_tdm_ref) {
    ch_offsets[selected_channels++] = ctx.tdm_ref_slot;
  }
  if (selected_channels != ctx.rx_rate_converter_channels) {
    this->fail_audio_session_("TDM RX channel map");
    return false;
  }
  int16_t *ref_out = ctx.use_tdm_ref ? ctx.spk_ref_buffer : nullptr;
  const bool ok = ctx.i2s_bps == 4
                      ? this->rx_rate_converter_.process_multi_32(
                            reinterpret_cast<const int32_t *>(ctx.rx_buffer), ctx.input_frame_size, ts, ch_offsets,
                            dual_mic ? ctx.processor_mic_buffer : nullptr, ctx.mic_buffer, ref_out, num_mic_ch)
                      : this->rx_rate_converter_.process_multi(ctx.rx_buffer, ctx.input_frame_size, ts, ch_offsets,
                                                               dual_mic ? ctx.processor_mic_buffer : nullptr,
                                                               ctx.mic_buffer, ref_out, num_mic_ch);
  if (!ok) {
    this->fail_audio_session_("TDM RX audio-effects conversion");
    return false;
  }
  if (dual_mic) {
    ctx.processor_input = ctx.processor_mic_buffer;
  }
  return true;
}
#else
bool ESPAudioStack::process_rx_tdm_(AudioTaskCtx &ctx) {
  (void) ctx;
  this->fail_audio_session_("TDM RX unsupported");
  return false;
}
#endif

#ifdef USE_ESP_AUDIO_STACK_STEREO_REF
bool ESPAudioStack::process_rx_stereo_ref_(AudioTaskCtx &ctx) {
  const uint8_t mi = ctx.ref_channel_right ? 0 : 1;
  const uint8_t ri = ctx.ref_channel_right ? 1 : 0;
  uint8_t ch_offsets[2] = {mi, ri};
  const bool ok = ctx.i2s_bps == 4
                      ? this->rx_rate_converter_.process_multi_32(reinterpret_cast<const int32_t *>(ctx.rx_buffer),
                                                                  ctx.input_frame_size, 2, ch_offsets, nullptr,
                                                                  ctx.mic_buffer, ctx.spk_ref_buffer, 1)
                      : this->rx_rate_converter_.process_multi(ctx.rx_buffer, ctx.input_frame_size, 2, ch_offsets,
                                                               nullptr, ctx.mic_buffer, ctx.spk_ref_buffer, 1);
  if (!ok) {
    this->fail_audio_session_("stereo RX audio-effects conversion");
    return false;
  }
  return true;
}
#else
bool ESPAudioStack::process_rx_stereo_ref_(AudioTaskCtx &ctx) {
  (void) ctx;
  this->fail_audio_session_("stereo RX reference unsupported");
  return false;
}
#endif

#ifdef USE_ESP_AUDIO_STACK_MULTI_RX
bool ESPAudioStack::process_rx_stereo_slot_(AudioTaskCtx &ctx) {
  const uint8_t mic_offset = this->mic_channel_right_ ? 1 : 0;
  uint8_t ch_offsets[1] = {mic_offset};
  const bool ok = ctx.i2s_bps == 4
                      ? this->rx_rate_converter_.process_multi_32(reinterpret_cast<const int32_t *>(ctx.rx_buffer),
                                                                  ctx.input_frame_size, 2, ch_offsets, nullptr,
                                                                  ctx.mic_buffer, nullptr, 1)
                      : this->rx_rate_converter_.process_multi(ctx.rx_buffer, ctx.input_frame_size, 2, ch_offsets,
                                                               nullptr, ctx.mic_buffer, nullptr, 1);
  if (!ok) {
    this->fail_audio_session_(ctx.i2s_bps == 4 ? "stereo mic RX 32-bit audio-effects conversion"
                                               : "stereo mic RX audio-effects conversion");
    return false;
  }
  return true;
}
#else
bool ESPAudioStack::process_rx_stereo_slot_(AudioTaskCtx &ctx) {
  (void) ctx;
  this->fail_audio_session_("stereo mic RX unsupported");
  return false;
}
#endif

#ifdef USE_ESP_AUDIO_STACK_MONO_RX
bool ESPAudioStack::process_rx_mono_effects_(AudioTaskCtx &ctx) {
  bool ok = true;
  if (ctx.i2s_bps == 4) {
    ok = this->mic_rate_converter_.process_strided_32(reinterpret_cast<const int32_t *>(ctx.rx_buffer), ctx.mic_buffer,
                                                      ctx.input_frame_size, 1, 0);
  } else if (ctx.ratio > 1) {
    ok = this->mic_rate_converter_.process(ctx.rx_buffer, ctx.mic_buffer, ctx.bus_frame_size);
  }
  if (!ok) {
    this->fail_audio_session_(ctx.i2s_bps == 4 ? "mono RX 32-bit audio-effects conversion" : "mono RX rate conversion");
    return false;
  }
  return true;
}
#else
bool ESPAudioStack::process_rx_mono_effects_(AudioTaskCtx &ctx) {
  (void) ctx;
  this->fail_audio_session_("mono RX audio-effects unsupported");
  return false;
}
#endif

void ESPAudioStack::apply_input_conditioning_(AudioTaskCtx &ctx) {
  // Fused loop: DC offset + input gain staging in one pass when DC or positive
  // boost is needed. Pure input attenuation uses esp-audio-libs Q31 below.
  // For dual-mic: mic1 is in mic_buffer, mic2 is in processor_mic_buffer[i*2+1]
  // (both filled by the multi-channel rate converter). Apply DC+input gain on
  // both, update in-place. When neither DC nor input gain is needed,
  // processor_mic_buffer (dual_mic case) is left as-is: the multi-channel rate
  // converter has already produced correct values for both mics.
  const bool do_dc = ctx.correct_dc_offset;
  const bool do_input_gain_q31 = ctx.input_gain_q31 != INT32_MAX;
  const bool do_input_gain_boost = ctx.input_gain_boost != 1.0f;
  const bool dual_mic = ctx.processor_mic_channels > 1 && ctx.processor_mic_buffer != nullptr;

  if (do_dc || do_input_gain_boost) {
    const float boost = ctx.input_gain_boost;
    for (size_t i = 0; i < ctx.input_frame_size; i++) {
      int16_t s1 = ctx.mic_buffer[i];

      if (do_dc) {
        s1 = this->dc_primary_.process(s1);
      }
      if (do_input_gain_boost) {
        s1 = scale_sample(s1, boost);
      }
      ctx.mic_buffer[i] = s1;

      if (dual_mic) {
        // mic2 already in processor_mic_buffer interleaved by multi-channel
        // rate conversion.
        int16_t s2 = ctx.processor_mic_buffer[i * 2 + 1];
        if (do_dc) {
          s2 = this->dc_secondary_.process(s2);
        }
        if (do_input_gain_boost) {
          s2 = scale_sample(s2, boost);
        }
        // Update both mic1 and mic2 in the interleaved buffer
        ctx.processor_mic_buffer[i * 2] = s1;
        ctx.processor_mic_buffer[i * 2 + 1] = s2;
      }
    }
  }

  if (do_input_gain_q31) {
    auto *mic_bytes = reinterpret_cast<uint8_t *>(ctx.mic_buffer);
    if (ctx.input_gain_q31 == 0) {
      memset(mic_bytes, 0, ctx.input_frame_size * sizeof(int16_t));
    } else {
      esp_audio_libs::gain::apply(mic_bytes, mic_bytes, ctx.input_gain_q31, ctx.input_frame_size, sizeof(int16_t));
    }
    if (dual_mic) {
      auto *processor_bytes = reinterpret_cast<uint8_t *>(ctx.processor_mic_buffer);
      const size_t processor_bytes_len = ctx.input_frame_size * ctx.processor_mic_channels * sizeof(int16_t);
      if (ctx.input_gain_q31 == 0) {
        memset(processor_bytes, 0, processor_bytes_len);
      } else {
        esp_audio_libs::gain::apply(processor_bytes, processor_bytes, ctx.input_gain_q31,
                                    ctx.input_frame_size * ctx.processor_mic_channels, sizeof(int16_t));
      }
    }
  }
  // dual_mic with no DC/input gain: processor_mic_buffer already correct from
  // the rate converter.
}

#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
void ESPAudioStack::update_tdm_slot_levels_(const AudioTaskCtx &ctx) {
  uint8_t enabled_slots[8];
  size_t enabled_count = 0;
  const uint8_t slot_limit = std::min<uint8_t>(ctx.tdm_total_slots, 8);
  for (uint8_t slot = 0; slot < slot_limit; slot++) {
    if (this->tdm_slot_level_sensor_enabled_[slot]) {
      enabled_slots[enabled_count++] = slot;
    }
  }
  if (enabled_count == 0) {
    return;
  }

  // Probe only every ~256 ms at 16 kHz / 512-sample cadence to keep overhead
  // low.
  this->tdm_slot_level_divider_++;
  if (this->tdm_slot_level_divider_ < 8) {
    return;
  }
  this->tdm_slot_level_divider_ = 0;

  const size_t frame_samples = ctx.bus_frame_size;
  const size_t slot_stride = ctx.tdm_total_slots;
  for (size_t i = 0; i < enabled_count; i++) {
    uint8_t slot = enabled_slots[i];
    float dbfs;
    if (ctx.i2s_bps == 4) {
      // 32-bit mode: rx_buffer stays native; esp_audio_effects handles bit
      // conversion later.
      auto *src32 = reinterpret_cast<const int32_t *>(ctx.rx_buffer);
      dbfs = compute_rms_dbfs_i32_top16(src32 + slot, frame_samples, slot_stride);
    } else {
      dbfs = compute_rms_dbfs_i16(ctx.rx_buffer + slot, frame_samples, slot_stride);
    }
    this->tdm_slot_level_dbfs_[slot].store(dbfs, std::memory_order_relaxed);
  }
}
#endif

#ifdef USE_AUDIO_PROCESSOR
void ESPAudioStack::run_processor_(AudioTaskCtx &ctx) {
  this->processor_->process(ctx.processor_input, ctx.spk_ref_buffer, ctx.aec_output, ctx.processor_mic_channels);
  ctx.output_buffer = ctx.aec_output;
  ctx.current_output_frame_size = ctx.output_frame_size;
  ctx.current_output_frame_bytes = ctx.output_frame_bytes;
}
#endif

// ════════════════════════════════════════════════════════════════════════════
// AEC/AFE + CALLBACKS: processing → gain → post-processor callbacks
// ════════════════════════════════════════════════════════════════════════════
bool ESPAudioStack::can_dispatch_mic_frame_(const AudioTaskCtx &ctx) const {
  return this->rx_handle_ != nullptr && ctx.output_buffer != nullptr && ctx.mic_running;
}

void ESPAudioStack::process_aec_and_callbacks_(AudioTaskCtx &ctx) {
  if (!this->can_dispatch_mic_frame_(ctx))
    return;

#ifdef USE_AUDIO_PROCESSOR
  if (ctx.processor_enabled && (!ctx.processor_ready || ctx.spk_ref_buffer == nullptr || ctx.aec_output == nullptr)) {
    // Processor configured but unavailable: fail closed. Production consumers
    // must never receive implicit raw mic audio from a configured processor
    // path during init, teardown, reinit, or allocation failure.
    if (ctx.aec_output != nullptr) {
      memset(ctx.aec_output, 0, ctx.output_frame_bytes);
      ctx.output_buffer = ctx.aec_output;
      ctx.current_output_frame_size = ctx.output_frame_size;
      ctx.current_output_frame_bytes = ctx.output_frame_bytes;
    } else {
      memset(ctx.output_buffer, 0, ctx.current_output_frame_bytes);
    }
  } else {
#if SOC_I2S_SUPPORTS_TDM && defined(USE_ESP_AUDIO_STACK_TDM_REF)
    if (ctx.use_tdm_ref && ctx.processor_ready && ctx.spk_ref_buffer != nullptr && ctx.aec_output != nullptr) {
      // TDM: hardware-synced reference, no speaker gating needed.
#ifdef USE_ESP_AUDIO_STACK_TDM_REF_DIAGNOSTIC
      // Health monitor for slot-map bring-up. Kept out of production builds:
      // it computes speaker/reference RMS during playback and carries verbose
      // diagnostic strings that are not part of the normal audio path.
      constexpr float kRefSilenceThresholdDbfs = -60.0f;
      constexpr float kSpeakerActiveThresholdDbfs = -55.0f;
      constexpr uint32_t kRefSilenceWarnFrames = 100;  // ~3.2s @ 32ms frames
      const bool spk_frame_has_audio =
          ctx.previous_speaker_got == ctx.bus_frame_bytes && ctx.previous_speaker_dbfs > kSpeakerActiveThresholdDbfs;
      if (spk_frame_has_audio) {
        const float ref_dbfs = compute_rms_dbfs_i16(ctx.spk_ref_buffer, ctx.input_frame_size, 1);
        auto log_tdm_ref_monitor = [&](const char *label, uint32_t silent_frames) {
          float raw_slot_dbfs[4] = {-120.0f, -120.0f, -120.0f, -120.0f};
          const uint8_t raw_slot_count = std::min<uint8_t>(ctx.tdm_total_slots, 4);
          for (uint8_t slot = 0; slot < raw_slot_count; slot++) {
            if (ctx.i2s_bps == 4) {
              auto *src32 = reinterpret_cast<const int32_t *>(ctx.rx_buffer);
              raw_slot_dbfs[slot] = compute_rms_dbfs_i32_top16(src32 + slot, ctx.bus_frame_size, ctx.tdm_total_slots);
            } else {
              raw_slot_dbfs[slot] = compute_rms_dbfs_i16(ctx.rx_buffer + slot, ctx.bus_frame_size, ctx.tdm_total_slots);
            }
          }
          ESP_LOGW(TAG,
                   "%s: post_ref=%.1f dBFS silent_frames=%u "
                   "raw_slots=[0:%.1f 1:%.1f 2:%.1f 3:%.1f] "
                   "mic_slots=[%u,%d] ref_slot=%u prev_speaker=%.1f dBFS (%u/%u)",
                   label, ref_dbfs, (unsigned) silent_frames, raw_slot_dbfs[0], raw_slot_dbfs[1], raw_slot_dbfs[2],
                   raw_slot_dbfs[3], (unsigned) ctx.tdm_mic_slot, (int) ctx.tdm_second_mic_slot,
                   (unsigned) ctx.tdm_ref_slot, ctx.previous_speaker_dbfs, (unsigned) ctx.previous_speaker_got,
                   (unsigned) ctx.bus_frame_bytes);
        };
        if (!ctx.tdm_ref_active_monitor_logged) {
          log_tdm_ref_monitor("TDM ref active frame", 0);
          ctx.tdm_ref_active_monitor_logged = true;
        }
        if (ref_dbfs < kRefSilenceThresholdDbfs) {
          uint32_t n = this->tdm_ref_silent_frames_.fetch_add(1, std::memory_order_relaxed) + 1;
          if (n == kRefSilenceWarnFrames) {
            log_tdm_ref_monitor("TDM ref monitor", n);
          }
        } else {
          this->tdm_ref_silent_frames_.store(0, std::memory_order_relaxed);
        }
      } else {
        this->tdm_ref_silent_frames_.store(0, std::memory_order_relaxed);
      }
#endif
      this->run_processor_(ctx);
    }
#endif
#if defined(USE_ESP_AUDIO_STACK_STEREO_REF) || defined(USE_ESP_AUDIO_STACK_MONO_REF)
    if (!ctx.use_tdm_ref && ctx.processor_ready && ctx.spk_ref_buffer != nullptr && ctx.aec_output != nullptr) {
      // Mono mode: get AEC reference (direct from TX or ring buffer).
      // Reference is post-volume PCM, no additional scaling (Espressif TYPE2
      // pattern). When playback is idle, fill_mono_aec_reference_() zero-fills
      // the ref and we still call the processor. That keeps MWW/VA/call
      // components on the processed surface instead of silently switching to
      // raw mic audio.
#ifdef USE_ESP_AUDIO_STACK_MONO_REF
      if (!ctx.use_stereo_aec_ref) {
        this->fill_mono_aec_reference_(ctx);
      }
#endif
      // Stereo mode: spk_ref_buffer already filled from deinterleave. No extra
      // scaling. TDM mode: spk_ref_buffer filled from TDM deinterleave. No
      // extra scaling.

      this->run_processor_(ctx);
    }
#endif
  }
#endif

  this->apply_post_processor_gain_(ctx);
  this->dispatch_mic_callbacks_(ctx);
}

void ESPAudioStack::apply_post_processor_gain_(AudioTaskCtx &ctx) {
  if (ctx.output_buffer == nullptr)
    return;

  if (ctx.mic_gain_q31 != INT32_MAX) {
    auto *output_bytes = reinterpret_cast<uint8_t *>(ctx.output_buffer);
    if (ctx.mic_gain_q31 == 0) {
      memset(output_bytes, 0, ctx.current_output_frame_bytes);
    } else {
      esp_audio_libs::gain::apply(output_bytes, output_bytes, ctx.mic_gain_q31, ctx.current_output_frame_size,
                                  sizeof(int16_t));
    }
  }
  if (ctx.mic_gain_boost_db > 0) {
    this->apply_mic_alc_gain_(ctx.output_buffer, ctx.current_output_frame_size, ctx.mic_gain_boost_db);
  }
}

void ESPAudioStack::dispatch_mic_callbacks_(const AudioTaskCtx &ctx) {
  if (!ctx.mic_running || ctx.output_buffer == nullptr)
    return;

  for (size_t i = 0; i < this->mic_callback_count_; i++) {
    const auto &slot = this->mic_callbacks_[i];
    if (slot.fn != nullptr) {
      slot.fn(slot.ctx, (const uint8_t *) ctx.output_buffer, ctx.current_output_frame_bytes);
    }
  }
}

bool ESPAudioStack::apply_mic_alc_gain_(int16_t *samples, size_t sample_count, int8_t gain_db) {
  if (samples == nullptr || sample_count == 0 || gain_db <= 0) {
    return true;
  }
  if (this->mic_alc_handle_ == nullptr) {
    ESP_LOGE(TAG, "mic ALC was not prepared before realtime processing");
    return false;
  }
  if (this->mic_alc_gain_db_ != gain_db) {
    const esp_ae_err_t gain_err =
        esp_ae_alc_set_gain(static_cast<esp_ae_alc_handle_t>(this->mic_alc_handle_), 0, gain_db);
    if (gain_err != ESP_AE_ERR_OK) {
      ESP_LOGE(TAG, "esp_ae_alc_set_gain mic failed: err=%d gain=%d", static_cast<int>(gain_err),
               static_cast<int>(gain_db));
      return false;
    }
    this->mic_alc_gain_db_ = gain_db;
  }

  const esp_ae_err_t err = esp_ae_alc_process(static_cast<esp_ae_alc_handle_t>(this->mic_alc_handle_),
                                              static_cast<uint32_t>(sample_count), samples, samples);
  if (err == ESP_AE_ERR_OK) {
    return true;
  }
  ESP_LOGE(TAG, "esp_ae_alc_process mic failed: err=%d samples=%u gain=%d", static_cast<int>(err),
           static_cast<unsigned>(sample_count), static_cast<int>(gain_db));
  return false;
}

#ifdef USE_ESP_AUDIO_STACK_MONO_REF
void ESPAudioStack::fill_mono_aec_reference_(AudioTaskCtx &ctx) {
  // The reference source holds already converted samples at the processor rate
  // (rate conversion happens once on the TX side in process_tx_path_).
  // The reference is the input side of the processor, so the unit is
  // input_frame_bytes (matches AudioProcessor::process expecting in_ref of
  // input_samples length).
  const size_t ref_bytes = ctx.input_frame_bytes;
#ifdef USE_ESP_AUDIO_STACK_RING_REF
  if (this->aec_ref_ring_buffer_) {
    if (this->aec_ref_ring_buffer_->available() >= ref_bytes) {
      this->aec_ref_ring_buffer_->read(ctx.spk_ref_buffer, ref_bytes, 0);
      return;
    }
  }
#endif
#ifdef USE_ESP_AUDIO_STACK_PREVIOUS_FRAME_REF
  if (this->direct_aec_ref_ != nullptr && this->direct_aec_ref_valid_) {
    memcpy(ctx.spk_ref_buffer, this->direct_aec_ref_, ref_bytes);
    return;
  }
#endif

  // No reference available: zero-fill so the processor still owns the output
  // surface instead of switching to raw mic audio.
  memset(ctx.spk_ref_buffer, 0, ref_bytes);
}
#endif

#ifdef USE_ESP_AUDIO_STACK_32BIT
bool ESPAudioStack::tx_bit_cvt_16_to_32_(uint8_t channels, const void *in, uint32_t sample_num, void *out) {
  if (channels == 0 || in == nullptr || out == nullptr) {
    ESP_LOGE(TAG, "invalid TX bit conversion args");
    return false;
  }
  if (this->tx_bit_cvt_handle_ == nullptr || this->tx_bit_cvt_channels_ != channels) {
    ESP_LOGE(TAG, "TX bit converter was not prepared before realtime processing (ch=%u)",
             static_cast<unsigned>(channels));
    return false;
  }

  const esp_ae_err_t err = esp_ae_bit_cvt_process(static_cast<esp_ae_bit_cvt_handle_t>(this->tx_bit_cvt_handle_),
                                                  sample_num, const_cast<void *>(in), out);
  if (err == ESP_AE_ERR_OK)
    return true;
  ESP_LOGE(TAG, "esp_ae_bit_cvt_process TX failed: err=%d samples=%u ch=%u", static_cast<int>(err),
           static_cast<unsigned>(sample_num), static_cast<unsigned>(channels));
  return false;
}
#endif

bool ESPAudioStack::format_tx_frame_(AudioTaskCtx &ctx, void **tx_data, size_t *tx_bytes) {
  const uint8_t tx_channels = ctx.use_tdm_bus ? ctx.tdm_total_slots : ctx.num_ch;
  int16_t *formatted16 = ctx.spk_buffer;
  bool tx_formatted = false;

#if SOC_I2S_SUPPORTS_TDM && defined(USE_ESP_AUDIO_STACK_TDM_BUS)
  if (ctx.use_tdm_bus) {
    if (ctx.tdm_tx_buffer == nullptr || ctx.tx_silence_buffer == nullptr || tx_channels == 0 || tx_channels > 8) {
      ESP_LOGE(TAG, "missing TDM TX audio-effects buffers");
      return false;
    }
    esp_ae_sample_t slot_samples[8]{};
    for (uint8_t slot = 0; slot < tx_channels; slot++) {
      slot_samples[slot] = ctx.tx_silence_buffer;
    }
    if (ctx.tdm_tx_slot >= tx_channels) {
      ESP_LOGE(TAG, "tdm_tx_slot %u out of range for %u slots", (unsigned) ctx.tdm_tx_slot, (unsigned) tx_channels);
      return false;
    }
    slot_samples[ctx.tdm_tx_slot] = ctx.spk_buffer;
    const esp_ae_err_t err = esp_ae_intlv_process(tx_channels, 16, static_cast<uint32_t>(ctx.bus_frame_size),
                                                  slot_samples, ctx.tdm_tx_buffer);
    if (err != ESP_AE_ERR_OK) {
      ESP_LOGE(TAG, "esp_ae_intlv_process TDM TX failed: err=%d", static_cast<int>(err));
      return false;
    }
    formatted16 = ctx.tdm_tx_buffer;
    tx_formatted = true;
  }
#endif
#ifdef USE_ESP_AUDIO_STACK_STEREO_TX
  if (!tx_formatted && ctx.num_ch == 2) {
    if (ctx.speaker_channels == 2) {
      // ESPHome speaker streams provide standard interleaved L/R PCM when the
      // speaker declares two channels. IDF STD stereo TX expects that same
      // layout, so no mono duplication or de/interleave step is needed here.
      formatted16 = ctx.spk_buffer;
    } else {
      if (ctx.tx_interleave_buffer == nullptr) {
        ESP_LOGE(TAG, "missing stereo TX audio-effects buffer");
        return false;
      }
      esp_ae_sample_t stereo_samples[2] = {ctx.spk_buffer, ctx.spk_buffer};
      const esp_ae_err_t err = esp_ae_intlv_process(2, 16, static_cast<uint32_t>(ctx.bus_frame_size), stereo_samples,
                                                    ctx.tx_interleave_buffer);
      if (err != ESP_AE_ERR_OK) {
        ESP_LOGE(TAG, "esp_ae_intlv_process stereo TX failed: err=%d", static_cast<int>(err));
        return false;
      }
      formatted16 = ctx.tx_interleave_buffer;
    }
    tx_formatted = true;
  }
#else
  (void) tx_formatted;
#endif

#ifdef USE_ESP_AUDIO_STACK_32BIT
  if (ctx.i2s_bps == 4) {
    if (ctx.tx_32_buffer == nullptr) {
      ESP_LOGE(TAG, "missing TX 32-bit audio-effects buffer");
      return false;
    }
    if (!this->tx_bit_cvt_16_to_32_(tx_channels, formatted16, static_cast<uint32_t>(ctx.bus_frame_size),
                                    ctx.tx_32_buffer)) {
      return false;
    }
    *tx_data = ctx.tx_32_buffer;
    *tx_bytes = ctx.bus_frame_size * tx_channels * sizeof(int32_t);
    return true;
  }
#endif

  *tx_data = formatted16;
  *tx_bytes = ctx.bus_frame_size * tx_channels * sizeof(int16_t);
  return true;
}

// ════════════════════════════════════════════════════════════════════════════
// TX PATH: ring buffer read → volume → Espressif format conversion → I2S write
// ════════════════════════════════════════════════════════════════════════════
bool ESPAudioStack::write_tx_frame_(AudioTaskCtx &ctx, void *tx_data, size_t tx_bytes) {
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  const bool write_ok = this->codec_backend_.write(tx_data, tx_bytes);
  const bool io_closed = !this->codec_backend_.is_open();
#else
  size_t bytes_written = 0;
  const esp_err_t write_err = i2s_channel_write(this->tx_handle_, tx_data, tx_bytes, &bytes_written, portMAX_DELAY);
  const bool write_ok = write_err == ESP_OK && bytes_written == tx_bytes;
  const bool io_closed = write_err == ESP_ERR_INVALID_STATE;
#endif
  if (!write_ok && io_closed) {
    if (this->audio_stack_running_.load(std::memory_order_relaxed)) {
      if (++ctx.invalid_state_errors > 100) {
        ESP_LOGE(TAG, "Persistent I2S TX INVALID_STATE (%d) - channel corrupted", ctx.invalid_state_errors);
        this->has_i2s_error_.store(true, std::memory_order_relaxed);
        this->audio_stack_running_.store(false, std::memory_order_relaxed);
      }
    }
    return false;
  }
  if (!write_ok) {
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
    LOG_W_THROTTLED("Codec TX write failed");
#else
    LOG_W_THROTTLED("I2S write failed: err=%s bytes=%u/%u", esp_err_to_name(write_err),
                    static_cast<unsigned>(bytes_written), static_cast<unsigned>(tx_bytes));
#endif
    if (++ctx.consecutive_i2s_errors > 100) {
      ESP_LOGE(TAG, "Persistent I2S write errors (%d)", ctx.consecutive_i2s_errors);
      this->has_i2s_error_.store(true, std::memory_order_relaxed);
      this->audio_stack_running_.store(false, std::memory_order_relaxed);
    }
    return false;
  }

  ctx.consecutive_i2s_errors = 0;
  ctx.invalid_state_errors = 0;
  return true;
}

bool ESPAudioStack::write_tx_dma_blocks_(AudioTaskCtx &ctx, void *tx_data, size_t tx_bytes, size_t real_speaker_bytes) {
  if (tx_data == nullptr || tx_bytes == 0) {
    return false;
  }
  if (this->tx_completion_dma_buffer_bytes_ == 0 || this->tx_completion_dma_frames_ == 0) {
    ESP_LOGE(TAG, "TX DMA completion tracking is not initialized");
    this->has_i2s_error_.store(true, std::memory_order_relaxed);
    this->audio_stack_running_.store(false, std::memory_order_relaxed);
    return false;
  }
  if (tx_bytes % this->tx_completion_dma_buffer_bytes_ != 0) {
    ESP_LOGE(TAG,
             "TX frame (%u bytes) is not aligned to DMA buffer (%u bytes); "
             "speaker playback callbacks cannot stay in lockstep",
             (unsigned) tx_bytes, (unsigned) this->tx_completion_dma_buffer_bytes_);
    this->has_i2s_error_.store(true, std::memory_order_relaxed);
    this->audio_stack_running_.store(false, std::memory_order_relaxed);
    return false;
  }

  const size_t public_frame_bytes = sizeof(int16_t) * static_cast<size_t>(ctx.speaker_channels);
  uint32_t remaining_real_frames =
      public_frame_bytes > 0 ? static_cast<uint32_t>(real_speaker_bytes / public_frame_bytes) : 0;
  auto *bytes = static_cast<uint8_t *>(tx_data);

#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  for (size_t offset = 0; offset < tx_bytes; offset += this->tx_completion_dma_buffer_bytes_) {
    const uint32_t real_frames = std::min(remaining_real_frames, this->tx_completion_dma_frames_);
    TxCompletionRecord record{};
    record.real_frames = real_frames;
    record.trailing_silence_frames = real_frames > 0 ? this->tx_completion_dma_frames_ - real_frames : 0;
    if (!this->queue_tx_completion_record_(record)) {
      return false;
    }
    remaining_real_frames -= real_frames;
  }
  return this->write_tx_frame_(ctx, tx_data, tx_bytes);
#else
  for (size_t offset = 0; offset < tx_bytes; offset += this->tx_completion_dma_buffer_bytes_) {
    const uint32_t real_frames = std::min(remaining_real_frames, this->tx_completion_dma_frames_);
    TxCompletionRecord record{};
    record.real_frames = real_frames;
    record.trailing_silence_frames = real_frames > 0 ? this->tx_completion_dma_frames_ - real_frames : 0;
    if (!this->queue_tx_completion_record_(record)) {
      return false;
    }
    if (!this->write_tx_frame_(ctx, bytes + offset, this->tx_completion_dma_buffer_bytes_)) {
      return false;
    }
    remaining_real_frames -= real_frames;
  }
  return true;
#endif
}

void ESPAudioStack::process_tx_clock_only_(AudioTaskCtx &ctx) {
  if (!this->tx_handle_ || ctx.tx_clock_buffer == nullptr)
    return;

  const size_t tx_bytes = ctx.use_tdm_bus ? ctx.tdm_tx_frame_bytes : ctx.bus_frame_size * ctx.num_ch * ctx.i2s_bps;
  ctx.speaker_got = 0;
  ctx.speaker_underrun = false;
#ifdef USE_ESP_AUDIO_STACK_TDM_REF_DIAGNOSTIC
  ctx.current_speaker_dbfs = -120.0f;
#endif
  // Clock-only TX keeps full-duplex RX/I2S clocks alive while no speaker
  // audio is playing. It contains no public speaker frames, but the I2S ISR
  // still emits TX completion events, so queue zero-frame records to keep the
  // completion stream in lockstep without notifying speaker consumers.
  this->write_tx_dma_blocks_(ctx, ctx.tx_clock_buffer, tx_bytes, 0);
}

void ESPAudioStack::process_tx_path_(AudioTaskCtx &ctx) {
  if (!this->tx_handle_)
    return;

  if (ctx.speaker_running && !ctx.speaker_paused) {
    ctx.speaker_got = this->speaker_buffer_->read((void *) ctx.spk_buffer, ctx.speaker_frame_bytes, 0);
    size_t got = ctx.speaker_got;
    // Treat partial frames as underrun too (the frame is padded with zero below
    // and must not be used as AEC reference, otherwise the AEC adaptive filter
    // sees a half-real / half-silent signal and fails to correlate with the
    // mic).
    ctx.speaker_underrun = got < ctx.speaker_frame_bytes;

    if (got > 0) {
      // Hot-path output volume is cached as Q31 when the volume changes, so
      // the audio task does not spend every frame converting float -> fixed.
      auto *spk_bytes = reinterpret_cast<uint8_t *>(ctx.spk_buffer);
      if (ctx.hot_output_volume_q31 == 0) {
        memset(spk_bytes, 0, got);
      } else if (ctx.hot_output_volume_q31 != INT32_MAX) {
        const size_t got_samples = got / sizeof(int16_t);
        esp_audio_libs::gain::apply(spk_bytes, spk_bytes, ctx.hot_output_volume_q31, got_samples, sizeof(int16_t));
      }
      if (got < ctx.speaker_frame_bytes) {
        memset(((uint8_t *) ctx.spk_buffer) + got, 0, ctx.speaker_frame_bytes - got);
      }
#ifdef USE_ESP_AUDIO_STACK_TDM_REF_DIAGNOSTIC
      ctx.current_speaker_dbfs = compute_rms_dbfs_i16(ctx.spk_buffer, ctx.bus_frame_size, 1);
#endif
    } else {
      memset(ctx.spk_buffer, 0, ctx.speaker_frame_bytes);
#ifdef USE_ESP_AUDIO_STACK_TDM_REF_DIAGNOSTIC
      ctx.current_speaker_dbfs = -120.0f;
#endif
    }
  } else {
    // Paused speaker output must not drain speaker_buffer_: ESPHome speaker
    // pause means processing incoming audio is suspended, not consumed as
    // silent playback.
    memset(ctx.spk_buffer, 0, ctx.speaker_frame_bytes);
    ctx.speaker_got = 0;
#ifdef USE_ESP_AUDIO_STACK_TDM_REF_DIAGNOSTIC
    ctx.current_speaker_dbfs = -120.0f;
#endif
  }

  // Save post-volume TX data as AEC reference (skip if processor is off).
  // The reference is converted to the processor rate HERE, on the TX side, so
  // downstream storage and consumer reads happen at the smaller output size.
  // Two safety properties of this gating:
  //   1) Decimation only runs on a complete frame (speaker_got ==
  //   speaker_frame_bytes),
  //      otherwise the converter state would absorb zero-padding and pollute
  //      the next valid frame's reference for ~32 samples.
  //   2) Skipping the save on a short read keeps the last good reference, same
  //      as the prior implementation.
#ifdef USE_AUDIO_PROCESSOR
#ifdef USE_ESP_AUDIO_STACK_MONO_REF
  const bool full_frame = ctx.speaker_running && !ctx.speaker_paused && ctx.speaker_got == ctx.speaker_frame_bytes;
  const int16_t *ref_source = ctx.spk_buffer;
#ifdef USE_ESP_AUDIO_STACK_STEREO_TX
  if (full_frame && !ctx.use_tdm_bus && ctx.speaker_channels > 1) {
    if (ctx.tx_ref_mono_buffer == nullptr || this->tx_ref_ch_cvt_handle_ == nullptr) {
      ESP_LOGE(TAG, "missing stereo TX mono reference converter");
      this->has_i2s_error_.store(true, std::memory_order_relaxed);
      this->audio_stack_running_.store(false, std::memory_order_relaxed);
      return;
    }
    const esp_ae_err_t err =
        esp_ae_ch_cvt_process(static_cast<esp_ae_ch_cvt_handle_t>(this->tx_ref_ch_cvt_handle_),
                              static_cast<uint32_t>(ctx.bus_frame_size), ctx.spk_buffer, ctx.tx_ref_mono_buffer);
    if (err != ESP_AE_ERR_OK) {
      ESP_LOGE(TAG, "esp_ae_ch_cvt_process TX ref failed: err=%d", static_cast<int>(err));
      this->has_i2s_error_.store(true, std::memory_order_relaxed);
      this->audio_stack_running_.store(false, std::memory_order_relaxed);
      return;
    }
    ref_source = ctx.tx_ref_mono_buffer;
  }
#endif
#ifdef USE_ESP_AUDIO_STACK_RING_REF
  if (this->aec_ref_ring_buffer_ && ctx.processor_enabled && ctx.mic_running) {
    if (full_frame && this->direct_aec_ref_ != nullptr) {
      // Decimate TX -> processor rate into direct_aec_ref_ scratch, then push
      // the converted frame into the ring. direct_aec_ref_ is sized for
      // input_frame_bytes by allocate_audio_buffers_() (the converter writes
      // bus_frame_size / ratio = input_frame_size samples).
      if (!this->play_ref_rate_converter_.process(ref_source, this->direct_aec_ref_, ctx.bus_frame_size)) {
        ESP_LOGE(TAG, "TX AEC reference rate conversion failed; stopping audio session");
        this->has_i2s_error_.store(true, std::memory_order_relaxed);
        this->audio_stack_running_.store(false, std::memory_order_relaxed);
        return;
      }
      const size_t ref_bytes = ctx.input_frame_bytes;
      // ESPHome RingBuffer::write() already drops oldest data on overflow
      // (discard_bytes_ + write_without_replacement), which is the right
      // backpressure here: keep the most recent reference window for AEC.
      this->aec_ref_ring_buffer_->write((void *) this->direct_aec_ref_, ref_bytes);
    }
  }
#endif
#ifdef USE_ESP_AUDIO_STACK_PREVIOUS_FRAME_REF
  if (this->direct_aec_ref_ != nullptr && ctx.processor_enabled) {
    // Previous frame mode: convert TX once and keep the result for the next
    // AEC iteration. Only on a full frame, otherwise we keep the last good
    // direct_aec_ref_ to avoid feeding a zero-padded reference.
    if (full_frame) {
      if (!this->play_ref_rate_converter_.process(ref_source, this->direct_aec_ref_, ctx.bus_frame_size)) {
        ESP_LOGE(TAG, "TX AEC reference rate conversion failed; stopping audio session");
        this->has_i2s_error_.store(true, std::memory_order_relaxed);
        this->audio_stack_running_.store(false, std::memory_order_relaxed);
        return;
      }
      this->direct_aec_ref_valid_ = true;
    }
  }
#endif
#endif
#endif

  void *tx_data;
  size_t tx_bytes;
  if (!this->format_tx_frame_(ctx, &tx_data, &tx_bytes)) {
    ESP_LOGE(TAG, "TX audio-effects format conversion failed; stopping audio session");
    this->has_i2s_error_.store(true, std::memory_order_relaxed);
    this->audio_stack_running_.store(false, std::memory_order_relaxed);
    return;
  }

  this->write_tx_dma_blocks_(ctx, tx_data, tx_bytes, ctx.speaker_got);
}

#endif  // esp-audio-libs headers available

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32
