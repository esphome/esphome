#include "esp_afe.h"
#include "../esp_audio_stack/audio_core_audio_utils.h"
#include "../esp_audio_stack/audio_core_scoped_lock.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <esp_heap_caps.h>
#ifdef USE_ESP_AFE_GMF_PATH
#include <esp_gmf_obj.h>
#endif
#include <esp_timer.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace esphome {
namespace esp_afe {

static const char *const TAG = "esp_afe";
static const TickType_t CONFIG_MUTEX_TIMEOUT = pdMS_TO_TICKS(250);
// Max wait for process() to finish its current frame when a config change
// needs to rebuild the AFE instance. ~1.5 frame periods at 16 kHz / 512 samples.
static const TickType_t DRAIN_WAIT_TIMEOUT = pdMS_TO_TICKS(50);
static constexpr uint32_t REINIT_STAGE_WARN_US = 50000;

struct AfeModePreset {
  const char *name;
  int type;
  int mode;
};

static constexpr AfeModePreset AFE_MODE_PRESETS[] = {
    {"sr_low_cost", AFE_TYPE_SR, AFE_MODE_LOW_COST},         {"sr_high_perf", AFE_TYPE_SR, AFE_MODE_HIGH_PERF},
    {"voip_low_cost", AFE_TYPE_VC, AFE_MODE_LOW_COST},       {"voip_high_perf", AFE_TYPE_VC, AFE_MODE_HIGH_PERF},
    {"fd_low_cost", 3 /* AFE_TYPE_FD */, AFE_MODE_LOW_COST}, {"fd_high_perf", 3 /* AFE_TYPE_FD */, AFE_MODE_HIGH_PERF},
};

#if defined(USE_ESP_AUDIO_STACK_TELEMETRY) && ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
static constexpr bool ESP_AFE_TIMING_TELEMETRY = true;
#else
static constexpr bool ESP_AFE_TIMING_TELEMETRY = false;
#endif

[[maybe_unused]] static const char *aec_nlp_level_name(int level) {
  switch (level) {
    case 0:
      return "NORMAL";
    case 1:
      return "AGGRESSIVE";
    case 2:
      return "VERY_AGGRESSIVE";
    default:
      return "UNKNOWN";
  }
}

void EspAfe::set_input_format_override(const char *fmt) {
  if (fmt == nullptr || fmt[0] == '\0') {
    this->input_format_override_[0] = '\0';
    return;
  }
  std::strncpy(this->input_format_override_, fmt, sizeof(this->input_format_override_) - 1);
  this->input_format_override_[sizeof(this->input_format_override_) - 1] = '\0';
}

#if defined(USE_ESP_AUDIO_STACK_TELEMETRY) && ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
static constexpr bool ESP_AFE_DIAGNOSTIC_COUNTERS = true;
#else
static constexpr bool ESP_AFE_DIAGNOSTIC_COUNTERS = false;
#endif

static inline void diag_add(std::atomic<uint32_t> &counter, uint32_t value = 1) {
  if constexpr (ESP_AFE_DIAGNOSTIC_COUNTERS) {
    counter.fetch_add(value, std::memory_order_relaxed);
  } else {
    (void) counter;
    (void) value;
  }
}

static inline uint32_t diag_increment_and_get(std::atomic<uint32_t> &counter) {
  if constexpr (ESP_AFE_DIAGNOSTIC_COUNTERS) {
    return counter.fetch_add(1, std::memory_order_relaxed) + 1;
  } else {
    (void) counter;
    return 0;
  }
}

static inline void update_peak_atomic(std::atomic<uint32_t> &peak, uint32_t value) {
  if constexpr (ESP_AFE_DIAGNOSTIC_COUNTERS) {
    uint32_t current = peak.load(std::memory_order_relaxed);
    while (value > current &&
           !peak.compare_exchange_weak(current, value, std::memory_order_relaxed, std::memory_order_relaxed)) {
    }
  } else {
    (void) peak;
    (void) value;
  }
}

static inline void decrement_if_nonzero(std::atomic<uint32_t> &counter) {
  if constexpr (ESP_AFE_DIAGNOSTIC_COUNTERS) {
    uint32_t current = counter.load(std::memory_order_relaxed);
    while (current > 0 &&
           !counter.compare_exchange_weak(current, current - 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
    }
  } else {
    (void) counter;
  }
}

void EspAfe::log_memory_snapshot_(const char *label) const {
  ESP_LOGI(TAG, "Memory[%s]: internal_free=%u largest_internal=%u dma_free=%u largest_dma=%u psram_free=%u", label,
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_DMA),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static inline void silence_frame(int16_t *out, int output_samples) {
  if (out != nullptr && output_samples > 0) {
    memset(out, 0, static_cast<size_t>(output_samples) * sizeof(int16_t));
  }
}

static inline int16_t afe_ref_sample(const int16_t *in_ref, int i) { return in_ref != nullptr ? in_ref[i] : 0; }

static inline void stage_afe_input_frame(int16_t *dst, const int16_t *in_mic, const int16_t *in_ref, int samples,
                                         int transport_mic_channels, int afe_mic_channels, int total_channels) {
  if (dst == nullptr || in_mic == nullptr || samples <= 0 || total_channels <= 0) {
    return;
  }
  const int mic_stride = std::max(1, transport_mic_channels);

  if (afe_mic_channels == 1) {
    if (total_channels == 2) {
      for (int i = 0; i < samples; i++) {
        dst[0] = in_mic[i * mic_stride];
        dst[1] = afe_ref_sample(in_ref, i);
        dst += 2;
      }
      return;
    }
    if (total_channels == 3) {
      for (int i = 0; i < samples; i++) {
        dst[0] = in_mic[i * mic_stride];
        dst[1] = 0;
        dst[2] = afe_ref_sample(in_ref, i);
        dst += 3;
      }
      return;
    }
  } else if (transport_mic_channels >= 2) {
    if (total_channels == 3) {
      for (int i = 0; i < samples; i++) {
        const int base = i * mic_stride;
        dst[0] = in_mic[base];
        dst[1] = in_mic[base + 1];
        dst[2] = afe_ref_sample(in_ref, i);
        dst += 3;
      }
      return;
    }
    if (total_channels == 4) {
      for (int i = 0; i < samples; i++) {
        const int base = i * mic_stride;
        dst[0] = in_mic[base];
        dst[1] = in_mic[base + 1];
        dst[2] = 0;
        dst[3] = afe_ref_sample(in_ref, i);
        dst += 4;
      }
      return;
    }
  } else {
    if (total_channels == 3) {
      for (int i = 0; i < samples; i++) {
        dst[0] = in_mic[i];
        dst[1] = 0;
        dst[2] = afe_ref_sample(in_ref, i);
        dst += 3;
      }
      return;
    }
    if (total_channels == 4) {
      for (int i = 0; i < samples; i++) {
        dst[0] = in_mic[i];
        dst[1] = 0;
        dst[2] = 0;
        dst[3] = afe_ref_sample(in_ref, i);
        dst += 4;
      }
      return;
    }
  }

  for (int i = 0; i < samples; i++) {
    memset(dst, 0, static_cast<size_t>(total_channels) * sizeof(int16_t));
    dst[0] = in_mic[i * mic_stride];
    if (afe_mic_channels >= 2 && total_channels >= 2) {
      dst[1] = transport_mic_channels >= 2 ? in_mic[i * mic_stride + 1] : 0;
    }
    const int ref_index = (afe_mic_channels >= 2) ? (total_channels >= 4 ? 3 : 2) : (total_channels >= 3 ? 2 : 1);
    if (ref_index < total_channels) {
      dst[ref_index] = afe_ref_sample(in_ref, i);
    }
    dst += total_channels;
  }
}

aec_mode_t EspAfe::derive_aec_mode_() const {
  const bool high = (this->afe_mode_ == AFE_MODE_HIGH_PERF);
  switch (this->afe_type_) {
    case AFE_TYPE_SR:
      return high ? AEC_MODE_SR_HIGH_PERF : AEC_MODE_SR_LOW_COST;
    case 3:  // AFE_TYPE_FD (esp-sr 2.4+, value matches the upstream enum)
      // AEC_MODE_FD_HIGH_PERF = 6, AEC_MODE_FD_LOW_COST = 5.
      return static_cast<aec_mode_t>(high ? 6 : 5);
    default:  // AFE_TYPE_VC
      return high ? AEC_MODE_VOIP_HIGH_PERF : AEC_MODE_VOIP_LOW_COST;
  }
}

int EspAfe::afe_mic_channels_() const {
  if (this->mic_num_ < 2) {
    return 1;
  }
  return 2;
}

const char *EspAfe::memory_alloc_mode_to_str_() const {
  switch (this->memory_alloc_mode_) {
    case AFE_MEMORY_ALLOC_MORE_INTERNAL:
      return "MORE_INTERNAL";
    case AFE_MEMORY_ALLOC_INTERNAL_PSRAM_BALANCE:
      return "INTERNAL_PSRAM_BALANCE";
    case AFE_MEMORY_ALLOC_MORE_PSRAM:
      return "MORE_PSRAM";
    default:
      return "UNKNOWN";
  }
}

bool EspAfe::build_instance_(AfeInstance *instance) {
  if (instance == nullptr) {
    return false;
  }

  const int afe_mic_channels = this->afe_mic_channels_();
  if (afe_mic_channels >= 2) {
    this->se_enabled_.store(true, std::memory_order_relaxed);
  }
  // Stack-allocated input format string. Default preserves the established
  // "MR" / "MMR" shape; an optional override selects Espressif's documented
  // "MMNR" dual-mic layout without changing the transport-facing mic channel
  // count.
  char fmt[5];
  if (afe_mic_channels >= 2 && this->input_format_override_[0] != '\0') {
    std::strncpy(fmt, this->input_format_override_, sizeof(fmt) - 1);
    fmt[sizeof(fmt) - 1] = '\0';
  } else {
    for (int i = 0; i < afe_mic_channels && i < 2; i++)
      fmt[i] = 'M';
    fmt[afe_mic_channels] = 'R';
    fmt[afe_mic_channels + 1] = '\0';
  }

  afe_config_t *cfg =
      afe_config_init(fmt, nullptr, static_cast<afe_type_t>(this->afe_type_), static_cast<afe_mode_t>(this->afe_mode_));

  if (cfg == nullptr) {
    ESP_LOGE(TAG, "afe_config_init failed for input_format=%s type=%d mode=%d", fmt, this->afe_type_, this->afe_mode_);
    return false;
  }

  // Single-mic ESP-SR direct AFE is not reliable after a live disable_aec():
  // fetch can keep timing out and downstream consumers receive silence until
  // reboot. Match Espressif's algorithm_stream contract instead: create the
  // instance with AEC structurally on/off. Dual-mic GMF keeps AEC initialized
  // so its manager can still apply feature toggles at runtime.
  cfg->aec_init = afe_mic_channels >= 2 || this->aec_enabled_.load(std::memory_order_relaxed);
  cfg->aec_filter_length = this->aec_filter_length_;
  cfg->aec_mode = this->derive_aec_mode_();
  cfg->aec_nlp_level = static_cast<aec_nlp_level_t>(this->aec_nlp_level_);

  // On dual-mic AFE builds SE/BSS is structural. Espressif's AFE contract
  // downgrades to first-mic-only when SE is disabled, so dual-mic devices keep
  // it initialized and enabled by design.
  if (afe_mic_channels >= 2) {
    this->se_enabled_.store(true, std::memory_order_relaxed);
  }
  cfg->se_init = afe_mic_channels >= 2;

  cfg->ns_init = this->ns_enabled_.load(std::memory_order_relaxed);
  cfg->ns_model_name = nullptr;
  cfg->afe_ns_mode = AFE_NS_MODE_WEBRTC;

  // ESP-SR/GMF can toggle VAD at runtime only if the VAD block exists in the
  // AFE instance. Keep it structurally initialized and gate the live state via
  // esp_gmf_afe_manager_enable_features() before the pipeline starts.
  cfg->vad_init = true;
  cfg->vad_mode = static_cast<vad_mode_t>(this->vad_mode_);
  cfg->vad_model_name = nullptr;
  cfg->vad_min_speech_ms = this->vad_min_speech_ms_;
  cfg->vad_min_noise_ms = this->vad_min_noise_ms_;
  cfg->vad_delay_ms = this->vad_delay_ms_;
  cfg->vad_mute_playback = this->vad_mute_playback_;
  cfg->vad_enable_channel_trigger = this->vad_enable_channel_trigger_;

  cfg->wakenet_init = false;
  cfg->wakenet_model_name = nullptr;
  cfg->wakenet_model_name_2 = nullptr;

  cfg->agc_init = this->agc_enabled_.load(std::memory_order_relaxed);
  cfg->agc_mode = AFE_AGC_MODE_WEBRTC;
  cfg->agc_compression_gain_db = this->agc_compression_gain_;
  cfg->agc_target_level_dbfs = this->agc_target_level_;

  // esp-sr spawns its internal worker using these fields. Single-mic MR
  // runs WebRTC NS/AGC in the feed path and needs extra internal queue
  // headroom; this matches the previously validated ESP-SR direct topology.
  cfg->afe_perferred_core = this->task_core_;
  cfg->afe_perferred_priority = this->task_priority_;
  cfg->afe_ringbuf_size = (afe_mic_channels <= 1 && this->ringbuf_size_ < 16) ? 16 : this->ringbuf_size_;
  cfg->memory_alloc_mode = static_cast<afe_memory_alloc_mode_t>(this->memory_alloc_mode_);
  cfg->afe_linear_gain = this->afe_linear_gain_;
  cfg->debug_init = false;
  // Single-mic MR was validated with fixed_first_channel=true. Keep dual-mic
  // on the current processed-output behavior so BSS/raw-output selection is
  // unchanged for WS3/P4.
  cfg->fixed_first_channel = afe_mic_channels <= 1;

  afe_config_check(cfg);

  const esp_afe_sr_iface_t *handle = esp_afe_handle_from_config(cfg);
  if (handle == nullptr) {
    ESP_LOGE(TAG, "esp_afe_handle_from_config returned NULL");
    afe_config_free(cfg);
    return false;
  }

  if (afe_mic_channels <= 1) {
#ifdef USE_ESP_AFE_DIRECT_PATH
    esp_afe_sr_data_t *direct_data = handle->create_from_config(cfg);
    if (direct_data == nullptr) {
      ESP_LOGE(TAG, "esp-sr AFE create_from_config failed for single-mic path");
      afe_config_free(cfg);
      return false;
    }

    int feed_chunksize = handle->get_feed_chunksize(direct_data);
    int fetch_chunksize = handle->get_fetch_chunksize(direct_data);
    int total_channels = handle->get_feed_channel_num(direct_data);
    if (feed_chunksize <= 0 || fetch_chunksize <= 0 || total_channels <= 0) {
      ESP_LOGE(TAG, "Invalid single-mic AFE frame shape feed=%d fetch=%d channels=%d", feed_chunksize, fetch_chunksize,
               total_channels);
      handle->destroy(direct_data);
      afe_config_free(cfg);
      return false;
    }

    const size_t feed_bytes =
        static_cast<size_t>(feed_chunksize) * static_cast<size_t>(total_channels) * sizeof(int16_t);
    const uint32_t feed_buf_caps =
        this->feed_buf_in_psram_ ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    int16_t *feed_buf = static_cast<int16_t *>(heap_caps_aligned_alloc(16, feed_bytes, feed_buf_caps));
    if (feed_buf == nullptr && this->feed_buf_in_psram_) {
      ESP_LOGW(TAG, "single-mic feed_buf (%u bytes) fell back to internal RAM", static_cast<unsigned>(feed_bytes));
      feed_buf = static_cast<int16_t *>(heap_caps_aligned_alloc(16, feed_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (feed_buf == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate single-mic feed buffer (%u bytes)", static_cast<unsigned>(feed_bytes));
      handle->destroy(direct_data);
      afe_config_free(cfg);
      return false;
    }

    instance->direct_iface = handle;
    instance->direct_data = direct_data;
    instance->config = cfg;
    instance->feed_buf = feed_buf;
    instance->feed_chunksize = feed_chunksize;
    instance->fetch_chunksize = fetch_chunksize;
    instance->process_chunksize = fetch_chunksize;
    instance->total_channels = total_channels;
    return true;
#else
    ESP_LOGE(TAG, "Single-mic AFE direct path was not compiled in");
    afe_config_free(cfg);
    return false;
#endif
  }

#ifdef USE_ESP_AFE_GMF_PATH
  esp_gmf_afe_manager_cfg_t manager_cfg{};
  manager_cfg.afe_cfg = cfg;
  manager_cfg.feed_task_setting.prio = this->feed_task_priority_;
  manager_cfg.feed_task_setting.core = this->feed_task_core_;
  manager_cfg.feed_task_setting.stack_size = this->feed_task_stack_size_;
  manager_cfg.fetch_task_setting.prio = this->fetch_task_priority_;
  manager_cfg.fetch_task_setting.core = this->fetch_task_core_;
  manager_cfg.fetch_task_setting.stack_size = this->fetch_task_stack_size_;
  manager_cfg.read_cb = nullptr;
  manager_cfg.read_ctx = nullptr;
  manager_cfg.result_cb = nullptr;
  manager_cfg.result_ctx = nullptr;

  esp_gmf_afe_manager_handle_t manager = nullptr;
  esp_gmf_err_t manager_ret = esp_gmf_afe_manager_create(&manager_cfg, &manager);
  if (manager_ret != ESP_GMF_ERR_OK || manager == nullptr) {
    ESP_LOGE(TAG, "esp_gmf_afe_manager_create failed (ret=%d)", static_cast<int>(manager_ret));
    afe_config_free(cfg);
    return false;
  }
  // DEFAULT_GMF_AFE_CFG uses C designated initializers in an order GCC rejects
  // in C++ mode, so keep the same official defaults but initialize in struct
  // declaration order.
  esp_gmf_afe_cfg_t gmf_afe_cfg{};
  gmf_afe_cfg.afe_manager = manager;
  // ESPHome handles Micro Wake Word and Voice Assistant state externally. Keep
  // the official AFE element's output latency at zero so the ESPHome-facing
  // bridge preserves the processor cadence expected by microphone consumers.
  gmf_afe_cfg.delay_samples = 0;
  gmf_afe_cfg.models = nullptr;
  gmf_afe_cfg.wakeup_time = ESP_GMF_AFE_DEFAULT_WAKEUP_TIME_MS;
  gmf_afe_cfg.wakeup_end = ESP_GMF_AFE_DEFAULT_WAKEUP_END_MS;
  gmf_afe_cfg.vcmd_detect_en = false;
  gmf_afe_cfg.vcmd_timeout = ESP_GMF_AFE_DEFAULT_VCMD_TIMEOUT_MS;
  gmf_afe_cfg.mn_language = "cn";
  gmf_afe_cfg.event_cb = &EspAfe::gmf_event_cb_;
  gmf_afe_cfg.event_ctx = this;
  esp_gmf_obj_handle_t element = nullptr;
  esp_gmf_err_t element_ret = esp_gmf_afe_init(&gmf_afe_cfg, &element);
  if (element_ret != ESP_GMF_ERR_OK || element == nullptr) {
    ESP_LOGE(TAG, "esp_gmf_afe_init failed (ret=%d)", static_cast<int>(element_ret));
    esp_gmf_afe_manager_destroy(manager);
    afe_config_free(cfg);
    return false;
  }

  esp_gmf_pipeline_handle_t pipeline = nullptr;
  esp_gmf_err_t pipeline_ret = esp_gmf_pipeline_create(&pipeline);
  if (pipeline_ret != ESP_GMF_ERR_OK || pipeline == nullptr) {
    ESP_LOGE(TAG, "esp_gmf_pipeline_create failed (ret=%d)", static_cast<int>(pipeline_ret));
    esp_gmf_obj_delete(element);
    esp_gmf_afe_manager_destroy(manager);
    afe_config_free(cfg);
    return false;
  }
  pipeline_ret = esp_gmf_pipeline_register_el(pipeline, static_cast<esp_gmf_element_handle_t>(element));
  if (pipeline_ret != ESP_GMF_ERR_OK) {
    ESP_LOGE(TAG, "esp_gmf_pipeline_register_el failed (ret=%d)", static_cast<int>(pipeline_ret));
    esp_gmf_obj_delete(element);
    esp_gmf_pipeline_destroy(pipeline);
    esp_gmf_afe_manager_destroy(manager);
    afe_config_free(cfg);
    return false;
  }

  esp_gmf_task_cfg_t task_cfg = DEFAULT_ESP_GMF_TASK_CONFIG();
  task_cfg.thread.prio = this->fetch_task_priority_;
  task_cfg.thread.core = this->fetch_task_core_;
  task_cfg.thread.stack = this->fetch_task_stack_size_;
  task_cfg.name = "esp_afe_gmf";
  esp_gmf_task_handle_t task = nullptr;
  esp_gmf_err_t task_ret = esp_gmf_task_init(&task_cfg, &task);
  if (task_ret != ESP_GMF_ERR_OK || task == nullptr) {
    ESP_LOGE(TAG, "esp_gmf_task_init failed (ret=%d)", static_cast<int>(task_ret));
    esp_gmf_pipeline_destroy(pipeline);
    esp_gmf_afe_manager_destroy(manager);
    afe_config_free(cfg);
    return false;
  }
  pipeline_ret = esp_gmf_pipeline_bind_task(pipeline, task);
  if (pipeline_ret != ESP_GMF_ERR_OK) {
    ESP_LOGE(TAG, "esp_gmf_pipeline_bind_task failed (ret=%d)", static_cast<int>(pipeline_ret));
    esp_gmf_task_deinit(task);
    esp_gmf_pipeline_destroy(pipeline);
    esp_gmf_afe_manager_destroy(manager);
    afe_config_free(cfg);
    return false;
  }

  size_t feed_chunk_size = 0;
  uint8_t total_channels_u8 = 0;
  if (esp_gmf_afe_manager_get_chunk_size(manager, &feed_chunk_size) != ESP_GMF_ERR_OK ||
      esp_gmf_afe_manager_get_input_ch_num(manager, &total_channels_u8) != ESP_GMF_ERR_OK || feed_chunk_size == 0 ||
      total_channels_u8 == 0) {
    ESP_LOGE(TAG, "Failed to query GMF AFE manager frame shape");
    esp_gmf_task_deinit(task);
    esp_gmf_pipeline_destroy(pipeline);
    esp_gmf_afe_manager_destroy(manager);
    afe_config_free(cfg);
    return false;
  }

  int feed_chunksize = static_cast<int>(feed_chunk_size);
  int fetch_chunksize = feed_chunksize;
  // process_chunksize is the input quantum exposed to consumers via
  // frame_spec().input_samples. Keep it at the fetch/output cadence so callers
  // deliver one output frame per audio loop. If esp-sr needs a larger feed
  // quantum (P4/SR BSS: feed=1024, fetch=512), process() stages multiple calls
  // internally before enqueueing one feed frame.
  int process_chunksize = fetch_chunksize;
  if (process_chunksize <= 0) {
    process_chunksize = (feed_chunksize > 0) ? feed_chunksize : 0;
  }
  // Use official API for feed channel count instead of config struct (more robust
  // if esp-sr changes internal channel mapping in future versions).
  int total_channels = static_cast<int>(total_channels_u8);
  const bool needs_feed_staging = process_chunksize != feed_chunksize;
  int16_t *feed_buf = nullptr;
  if (needs_feed_staging) {
    const size_t feed_bytes = static_cast<size_t>(feed_chunksize) * total_channels * sizeof(int16_t);
    const uint32_t feed_buf_caps =
        this->feed_buf_in_psram_ ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    feed_buf = static_cast<int16_t *>(heap_caps_aligned_alloc(16, feed_bytes, feed_buf_caps));
    if (feed_buf == nullptr && this->feed_buf_in_psram_) {
      ESP_LOGW(TAG, "staged feed_buf (%u bytes) fell back to internal RAM (PSRAM full/unavailable)",
               static_cast<unsigned>(feed_bytes));
      feed_buf = static_cast<int16_t *>(heap_caps_aligned_alloc(16, feed_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (feed_buf == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate staged feed buffer (%u bytes)", static_cast<unsigned>(feed_bytes));
      esp_gmf_task_deinit(task);
      esp_gmf_pipeline_destroy(pipeline);
      esp_gmf_afe_manager_destroy(manager);
      afe_config_free(cfg);
      return false;
    }
  }

  instance->manager = manager;
  instance->element = element;
  instance->pipeline = pipeline;
  instance->task = task;
  instance->config = cfg;
  instance->feed_buf = feed_buf;
  instance->feed_chunksize = feed_chunksize;
  instance->fetch_chunksize = fetch_chunksize;
  instance->process_chunksize = process_chunksize;
  instance->total_channels = total_channels;

  return true;
#else
  ESP_LOGE(TAG, "Dual-mic GMF AFE path was not compiled in");
  afe_config_free(cfg);
  return false;
#endif
}

void EspAfe::destroy_instance_(AfeInstance *instance) {
  if (instance == nullptr) {
    return;
  }
#ifdef USE_ESP_AFE_DIRECT_PATH
  this->stop_direct_fetch_task_();
  if (instance->direct_data != nullptr && instance->direct_iface != nullptr) {
    instance->direct_iface->destroy(instance->direct_data);
    instance->direct_data = nullptr;
    instance->direct_iface = nullptr;
  }
#endif
#ifdef USE_ESP_AFE_GMF_PATH
  if (instance->task != nullptr) {
    esp_gmf_task_deinit(instance->task);
    instance->task = nullptr;
  }
  if (instance->pipeline != nullptr) {
    esp_gmf_pipeline_destroy(instance->pipeline);
    instance->pipeline = nullptr;
    instance->element = nullptr;
  } else if (instance->element != nullptr) {
    esp_gmf_obj_delete(instance->element);
    instance->element = nullptr;
  }
  if (instance->manager != nullptr) {
    esp_gmf_afe_manager_destroy(instance->manager);
    instance->manager = nullptr;
  }
#endif
  if (instance->config != nullptr) {
    afe_config_free(instance->config);
    instance->config = nullptr;
  }
  if (instance->feed_buf != nullptr) {
    heap_caps_free(instance->feed_buf);
    instance->feed_buf = nullptr;
  }
  instance->feed_chunksize = 0;
  instance->fetch_chunksize = 0;
  instance->process_chunksize = 0;
  instance->total_channels = 0;
}

bool EspAfe::install_instance_(AfeInstance *instance) {
#ifdef USE_ESP_AFE_DIRECT_PATH
  this->direct_iface_ = instance->direct_iface;
  this->direct_data_ = instance->direct_data;
#endif
#ifdef USE_ESP_AFE_GMF_PATH
  this->afe_manager_ = instance->manager;
  this->afe_element_ = instance->element;
  this->afe_pipeline_ = instance->pipeline;
  this->afe_task_ = instance->task;
#endif
  this->afe_config_ = instance->config;
  this->feed_buf_ = instance->feed_buf;
  this->feed_chunksize_ = instance->feed_chunksize;
  this->fetch_chunksize_ = instance->fetch_chunksize;
  this->process_chunksize_ = instance->process_chunksize;
  this->total_channels_ = instance->total_channels;
  this->staged_input_samples_ = 0;

#ifdef USE_ESP_AFE_DIRECT_PATH
  instance->direct_iface = nullptr;
  instance->direct_data = nullptr;
#endif
#ifdef USE_ESP_AFE_GMF_PATH
  instance->manager = nullptr;
  instance->element = nullptr;
  instance->pipeline = nullptr;
  instance->task = nullptr;
#endif
  instance->config = nullptr;
  instance->feed_buf = nullptr;
  instance->feed_chunksize = 0;
  instance->fetch_chunksize = 0;
  instance->process_chunksize = 0;
  instance->total_channels = 0;

  auto cleanup_failed_install = [this]() -> bool {
    AfeInstance failed;
#ifdef USE_ESP_AFE_DIRECT_PATH
    failed.direct_iface = this->direct_iface_;
    failed.direct_data = this->direct_data_;
#endif
#ifdef USE_ESP_AFE_GMF_PATH
    failed.manager = this->afe_manager_;
    failed.element = this->afe_element_;
    failed.pipeline = this->afe_pipeline_;
    failed.task = this->afe_task_;
#endif
    failed.config = this->afe_config_;
    failed.feed_buf = this->feed_buf_;
    failed.feed_chunksize = this->feed_chunksize_;
    failed.fetch_chunksize = this->fetch_chunksize_;
    failed.process_chunksize = this->process_chunksize_;
    failed.total_channels = this->total_channels_;
#ifdef USE_ESP_AFE_DIRECT_PATH
    this->direct_iface_ = nullptr;
    this->direct_data_ = nullptr;
#endif
#ifdef USE_ESP_AFE_GMF_PATH
    this->afe_manager_ = nullptr;
    this->afe_element_ = nullptr;
    this->afe_pipeline_ = nullptr;
    this->afe_task_ = nullptr;
#endif
    this->afe_config_ = nullptr;
    this->afe_pipeline_running_ = false;
    this->afe_pipeline_paused_ = false;
    this->feed_buf_ = nullptr;
    this->feed_chunksize_ = 0;
    this->fetch_chunksize_ = 0;
    this->process_chunksize_ = 0;
    this->total_channels_ = 0;
    this->destroy_instance_(&failed);
    return false;
  };

  if (!this->prepare_runtime_()) {
    ESP_LOGE(TAG, "Failed to prepare AFE runtime buffers");
    return cleanup_failed_install();
  }

#ifdef USE_ESP_AFE_DIRECT_PATH
  if (this->direct_iface_ != nullptr && this->direct_data_ != nullptr) {
    if (this->direct_feed_signal_ == nullptr) {
      this->direct_feed_signal_ =
          xSemaphoreCreateCountingStatic(kDirectFeedSignalMaxCount, 0, &this->direct_feed_signal_storage_);
      if (this->direct_feed_signal_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create single-mic AFE feed signal");
        return cleanup_failed_install();
      }
    }

    if (this->afe_config_ != nullptr && this->afe_config_->vad_init &&
        !this->vad_enabled_.load(std::memory_order_relaxed)) {
      this->direct_iface_->disable_vad(this->direct_data_);
      this->direct_iface_->reset_vad(this->direct_data_);
      this->voice_present_.store(false, std::memory_order_relaxed);
    }

    if (this->processing_active_.load(std::memory_order_acquire)) {
      if (!this->start_pipeline_()) {
        ESP_LOGE(TAG, "Reconfigure: single-mic AFE failed to start");
        return cleanup_failed_install();
      }
    }
    ESP_LOGI(TAG, "AFE single-mic path: ESP-SR direct feed/fetch");
    return true;
  }
#endif

#ifdef USE_ESP_AFE_GMF_PATH
  const size_t feed_bytes = static_cast<size_t>(this->feed_chunksize_) * this->total_channels_ * sizeof(int16_t);
  const size_t output_bytes = static_cast<size_t>(this->fetch_chunksize_) * sizeof(int16_t);
  esp_gmf_port_handle_t in_port = static_cast<esp_gmf_port_handle_t>(
      NEW_ESP_GMF_PORT_IN_BYTE(reinterpret_cast<void *>(&EspAfe::gmf_input_acquire_cb_),
                               reinterpret_cast<void *>(&EspAfe::gmf_input_release_cb_), nullptr, this,
                               static_cast<int>(feed_bytes), ESP_GMF_MAX_DELAY));
  esp_gmf_port_handle_t out_port = static_cast<esp_gmf_port_handle_t>(NEW_ESP_GMF_PORT_OUT_BYTE(
      reinterpret_cast<void *>(&EspAfe::gmf_output_acquire_cb_),
      reinterpret_cast<void *>(&EspAfe::gmf_output_release_cb_), nullptr, this, static_cast<int>(output_bytes), 0));
  if (in_port == nullptr || out_port == nullptr) {
    ESP_LOGE(TAG, "Failed to register GMF AFE ports");
    if (in_port != nullptr) {
      esp_gmf_port_deinit(in_port);
    }
    if (out_port != nullptr) {
      esp_gmf_port_deinit(out_port);
    }
    return cleanup_failed_install();
  }
  if (esp_gmf_element_register_in_port(this->afe_element_, in_port) != ESP_GMF_ERR_OK) {
    ESP_LOGE(TAG, "Failed to register GMF AFE input port");
    esp_gmf_port_deinit(in_port);
    esp_gmf_port_deinit(out_port);
    return cleanup_failed_install();
  }
  if (esp_gmf_element_register_out_port(this->afe_element_, out_port) != ESP_GMF_ERR_OK) {
    ESP_LOGE(TAG, "Failed to register GMF AFE output port");
    esp_gmf_element_unregister_in_port(this->afe_element_, in_port);
    esp_gmf_port_deinit(out_port);
    return cleanup_failed_install();
  }
  esp_gmf_err_t load_ret = esp_gmf_pipeline_loading_jobs(this->afe_pipeline_);
  if (load_ret != ESP_GMF_ERR_OK) {
    ESP_LOGE(TAG, "GMF AFE pipeline loading_jobs failed (ret=%d)", static_cast<int>(load_ret));
    return cleanup_failed_install();
  }
  // GMF keeps AEC initialized for manager-level runtime toggles. Disable the
  // feature through the manager if config says off.
  if (!this->aec_enabled_.load(std::memory_order_relaxed)) {
    esp_gmf_afe_manager_enable_features(this->afe_manager_, ESP_AFE_FEATURE_AEC, false);
  }
  // VAD must stay enabled when the GMF AFE element opens. GMF creates its
  // wake/VAD monitor lock only when feat.vad is true during esp_gmf_afe_open();
  // disabling it here leaves wake_st_lock null, and a later VAD enable crashes
  // in esp_gmf_afe_result_proc()->wakeup_state_update().

  // Runtime buffers are already prepared above. Apply the configured feature
  // state before resuming GMF tasks so an active reconfigure never runs even
  // one frame with stale AEC/VAD state.
  if (this->processing_active_.load(std::memory_order_acquire)) {
    if (!this->start_pipeline_()) {
      ESP_LOGE(TAG, "Reconfigure: GMF AFE element failed to open");
      return cleanup_failed_install();
    }
  }

  return true;
#else
  ESP_LOGE(TAG, "No usable AFE backend was compiled for this instance");
  return cleanup_failed_install();
#endif
}

EspAfe::AfeInstance EspAfe::detach_instance_() {
  // Drain protocol has already quiesced process() before detach is called, so
  // no more enqueues can race. Close the official GMF AFE element first: this
  // detaches manager read/result callbacks and releases its data buses.
#ifdef USE_ESP_AFE_GMF_PATH
  if (!this->afe_pipeline_paused_) {
    this->flush_pipeline_before_stop_();
  }
#endif
  this->stop_pipeline_();

  AfeInstance instance;
#ifdef USE_ESP_AFE_DIRECT_PATH
  instance.direct_iface = this->direct_iface_;
  instance.direct_data = this->direct_data_;
#endif
#ifdef USE_ESP_AFE_GMF_PATH
  instance.manager = this->afe_manager_;
  instance.element = this->afe_element_;
  instance.pipeline = this->afe_pipeline_;
  instance.task = this->afe_task_;
#endif
  instance.config = this->afe_config_;
  instance.feed_buf = this->feed_buf_;
  instance.feed_chunksize = this->feed_chunksize_;
  instance.fetch_chunksize = this->fetch_chunksize_;
  instance.process_chunksize = this->process_chunksize_;
  instance.total_channels = this->total_channels_;

#ifdef USE_ESP_AFE_DIRECT_PATH
  this->direct_iface_ = nullptr;
  this->direct_data_ = nullptr;
#endif
#ifdef USE_ESP_AFE_GMF_PATH
  this->afe_manager_ = nullptr;
  this->afe_element_ = nullptr;
  this->afe_pipeline_ = nullptr;
  this->afe_task_ = nullptr;
#endif
  this->afe_config_ = nullptr;
  this->afe_pipeline_running_ = false;
  this->afe_pipeline_paused_ = false;
  this->feed_buf_ = nullptr;
  this->feed_chunksize_ = 0;
  this->fetch_chunksize_ = 0;
  this->process_chunksize_ = 0;
  this->total_channels_ = 0;
  this->staged_input_samples_ = 0;

  return instance;
}

void EspAfe::clear_process_busy_() {
  this->process_busy_.store(false, std::memory_order_seq_cst);
  TaskHandle_t waiter = this->process_drain_waiter_.load(std::memory_order_acquire);
  if (waiter != nullptr) {
    xTaskNotifyGive(waiter);
  }
}

bool EspAfe::recreate_instance_(bool require_same_frame_sizes) {
  const int64_t reinit_start_us = esp_timer_get_time();
  int64_t stage_start_us = reinit_start_us;
  auto begin_stage = [&](const char *stage) {
    (void) stage;
    stage_start_us = esp_timer_get_time();
  };
  auto log_stage = [&](const char *stage) {
    const int64_t now_us = esp_timer_get_time();
    const uint32_t stage_us = static_cast<uint32_t>(std::max<int64_t>(0, now_us - stage_start_us));
    if (stage_us >= REINIT_STAGE_WARN_US) {
      ESP_LOGW(TAG, "AFE reinit stage %s took %uus", stage, (unsigned) stage_us);
    }
    stage_start_us = now_us;
  };

  if (this->config_mutex_ == nullptr) {
    this->config_mutex_ = xSemaphoreCreateMutex();
    if (this->config_mutex_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create config mutex");
      return false;
    }
  }

  esp_audio_stack::ScopedLock lock(this->config_mutex_, CONFIG_MUTEX_TIMEOUT);
  if (!lock) {
    ESP_LOGW(TAG, "Timed out waiting to rebuild AFE instance");
    return false;
  }

  // Drain protocol: seq_cst gives the request/busy atomics one total order.
  // That prevents the store-buffer false/false outcome that release/acquire
  // would still allow on different atomic objects.
  this->drain_request_.store(true, std::memory_order_seq_cst);
  if (this->process_busy_.load(std::memory_order_seq_cst)) {
    TaskHandle_t waiter = xTaskGetCurrentTaskHandle();
    ulTaskNotifyTake(pdTRUE, 0);
    this->process_drain_waiter_.store(waiter, std::memory_order_release);
    if (this->process_busy_.load(std::memory_order_seq_cst) && ulTaskNotifyTake(pdTRUE, DRAIN_WAIT_TIMEOUT) == 0) {
      ESP_LOGW(TAG, "Drain timeout waiting for process() to quiesce, proceeding");
    }
    this->process_drain_waiter_.store(nullptr, std::memory_order_release);
  }
  log_stage("drain");

  // From here on the ScopedLock auto-releases on every return path; only the
  // drain flag has to be reset before each return.
  auto release_drain = [this]() { this->drain_request_.store(false, std::memory_order_seq_cst); };

  // esp-sr FFT resources are global: only one AFE instance can exist.
  // Must destroy the previous instance before creating the next one.
  int old_process = this->process_chunksize_;
  int old_fetch = this->fetch_chunksize_;
  begin_stage("detach");
  AfeInstance old = this->detach_instance_();
  log_stage("detach");
  begin_stage("destroy");
  this->destroy_instance_(&old);
  log_stage("destroy");
  begin_stage("release_runtime");
  this->release_runtime_buffers_();
  log_stage("release_runtime");

  // If every user-facing feature is disabled there is nothing to build.
  // Stay in the torn-down state; process() will emit silence via the
  // afe_stopped_ fast path using the last-known frame spec.
  //
  // Guard: only take this shortcut once we have a cached spec to hand out
  // via frame_spec(). On the very first call (setup() before any successful
  // install) last_spec_* are zero; build the instance normally so downstream
  // learns the frame shape, then subsequent toggles can tear down cleanly.
  if (this->all_features_disabled_() && this->last_spec_process_size_ > 0 && this->last_spec_fetch_size_ > 0) {
    bool was_running = !this->afe_stopped_.load(std::memory_order_acquire);
    this->afe_stopped_.store(true, std::memory_order_release);
    release_drain();
    if (was_running) {
      ESP_LOGI(TAG, "AFE stopped (all features disabled); output silenced");
    }
    return true;
  }

  AfeInstance next;
  begin_stage("build");
  if (!this->build_instance_(&next)) {
    log_stage("build_failed");
    ESP_LOGE(TAG, "Failed to build new AFE instance. AFE is DOWN until successful rebuild.");
    release_drain();
    return false;
  }
  log_stage("build");

  if (require_same_frame_sizes && old_process > 0 && old_fetch > 0 &&
      (next.process_chunksize != old_process || next.fetch_chunksize != old_fetch)) {
    ESP_LOGW(TAG, "Reinit changed external frame sizes (%d/%d -> %d/%d), rejecting", old_process, old_fetch,
             next.process_chunksize, next.fetch_chunksize);
    this->destroy_instance_(&next);
    release_drain();
    return false;
  }

  // Compare against the last successfully-installed spec, not against the
  // (already-detached) `this->*_chunksize_` fields which are zero at this
  // point. Using the last_spec_* members means a rollback to the previous
  // config does not spuriously bump frame_spec_revision_, which would make
  // esp_audio_stack try to restart its audio task concurrently with our
  // fetch task recreation and race inside FreeRTOS.
  int new_mic_ch = this->afe_mic_channels_();
  bool spec_changed =
      (new_mic_ch != this->last_spec_mic_ch_ || next.process_chunksize != this->last_spec_process_size_ ||
       next.fetch_chunksize != this->last_spec_fetch_size_);
  (void) old_process;
  (void) old_fetch;

  begin_stage("install");
  if (!this->install_instance_(&next)) {
    log_stage("install_failed");
    AfeInstance failed = this->detach_instance_();
    this->destroy_instance_(&failed);
    this->release_runtime_buffers_();
    release_drain();
    return false;
  }
  log_stage("install");

  this->last_spec_process_size_ = this->process_chunksize_;
  this->last_spec_fetch_size_ = this->fetch_chunksize_;

  if (spec_changed) {
    int old_mic_ch = this->last_spec_mic_ch_;
    this->last_spec_mic_ch_ = new_mic_ch;
    // Release barrier ensures new frame_spec stores happen-before consumers
    // observe the bumped revision via acquire load.
    uint32_t new_rev = this->frame_spec_revision_.fetch_add(1, std::memory_order_release) + 1;
    ESP_LOGI(TAG, "Frame spec changed: mic_ch=%d->%d, process=%d, fetch=%d (revision %u, audio task will restart)",
             old_mic_ch, new_mic_ch, this->process_chunksize_, this->fetch_chunksize_, (unsigned) new_rev);
  }

  // First boot with every single-mic feature disabled: build once so
  // frame_spec() has a valid shape, then tear the manager back down. This
  // prevents an all-off AFE from acting like a raw passthrough.
  if (this->all_features_disabled_()) {
    AfeInstance idle = this->detach_instance_();
    this->destroy_instance_(&idle);
    this->release_runtime_buffers_();
    this->afe_stopped_.store(true, std::memory_order_release);
    release_drain();
    ESP_LOGI(TAG, "AFE stopped (all features disabled); output silenced");
    return true;
  }

  this->warmup_remaining_ = 3;
  this->input_ring_drop_.store(0, std::memory_order_relaxed);
  this->feed_ok_.store(0, std::memory_order_relaxed);
  this->feed_rejected_.store(0, std::memory_order_relaxed);
  this->fetch_ok_.store(0, std::memory_order_relaxed);
  this->fetch_timeout_.store(0, std::memory_order_relaxed);
  this->output_ring_drop_.store(0, std::memory_order_relaxed);
  this->feed_queue_frames_.store(0, std::memory_order_relaxed);
  this->feed_queue_peak_.store(0, std::memory_order_relaxed);
  this->fetch_queue_frames_.store(0, std::memory_order_relaxed);
  this->fetch_queue_peak_.store(0, std::memory_order_relaxed);
  this->process_us_last_.store(0, std::memory_order_relaxed);
  this->process_us_max_.store(0, std::memory_order_relaxed);
  this->feed_us_last_.store(0, std::memory_order_relaxed);
  this->feed_us_max_.store(0, std::memory_order_relaxed);
  this->fetch_us_last_.store(0, std::memory_order_relaxed);
  this->fetch_us_max_.store(0, std::memory_order_relaxed);
  this->ringbuf_free_pct_.store(1.0f, std::memory_order_relaxed);
  this->voice_present_.store(false, std::memory_order_relaxed);
  this->input_volume_dbfs_.store(-120.0f, std::memory_order_relaxed);
  this->output_rms_dbfs_.store(-120.0f, std::memory_order_relaxed);
  // Clear the stopped flag: a live instance is running. Paired with the
  // acquire load in process() so the hot path sees the transition cleanly.
  bool was_stopped = this->afe_stopped_.exchange(false, std::memory_order_release);
  if (was_stopped) {
    ESP_LOGI(TAG, "AFE restarted (feature re-enabled)");
  }
  // Release drain: new instance is fully installed. The GMF pipeline/task
  // already exists and is either started by install_instance_ or left stopped
  // until the next mic consumer attaches. process() can resume real work on
  // the next frame.
  release_drain();
  const uint32_t total_us = static_cast<uint32_t>(std::max<int64_t>(0, esp_timer_get_time() - reinit_start_us));
  if (total_us >= REINIT_STAGE_WARN_US) {
    ESP_LOGW(TAG, "AFE reinit total took %uus (type=%d mode=%d)", (unsigned) total_us, this->afe_type_,
             this->afe_mode_);
  }
  return true;
}

bool EspAfe::set_aec_enabled_runtime_(bool enabled) {
  if (this->aec_enabled_.load(std::memory_order_relaxed) == enabled) {
    return true;
  }
  if (this->config_mutex_ == nullptr) {
    ESP_LOGW(TAG, "AEC toggle requested before setup");
    return false;
  }

  // AFE currently torn down (all features were off). Flip the flag and let
  // recreate_instance_ rebuild (or stay stopped if still all-off).
  if (this->afe_stopped_.load(std::memory_order_acquire)) {
    this->aec_enabled_.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
      return true;  // nothing to rebuild
    }
    return this->recreate_instance_(false);
  }

  if (!this->is_initialized()) {
    ESP_LOGW(TAG, "AEC toggle requested before initialization");
    return false;
  }

#ifdef USE_ESP_AFE_DIRECT_PATH
  if (this->direct_iface_ != nullptr && this->direct_data_ != nullptr) {
    bool old_value = this->aec_enabled_.load(std::memory_order_relaxed);
    this->aec_enabled_.store(enabled, std::memory_order_relaxed);
    ESP_LOGI(TAG, "Applying aec_enabled=%s (single-mic AFE rebuild)", enabled ? "true" : "false");
    if (this->recreate_instance_(false)) {
      return true;
    }
    ESP_LOGW(TAG, "Failed to apply aec_enabled=%s, rolling back", enabled ? "true" : "false");
    this->aec_enabled_.store(old_value, std::memory_order_relaxed);
    if (!this->recreate_instance_(false)) {
      ESP_LOGE(TAG, "Rollback also failed for aec_enabled, AFE is down");
    }
    return false;
  }
#endif

  // Hold the config mutex only across the enable/disable call. The
  // potential teardown via recreate_instance_ takes the same mutex
  // itself; calling it inside the lock would recurse on a non-recursive
  // mutex and time out.
  bool needs_rebuild = false;
  {
    esp_audio_stack::ScopedLock lock(this->config_mutex_, CONFIG_MUTEX_TIMEOUT);
    if (!lock) {
      ESP_LOGW(TAG, "Timed out waiting to toggle AEC");
      return false;
    }

    int ret = 0;
    const char *backend = "unknown";
#ifdef USE_ESP_AFE_DIRECT_PATH
    if (this->direct_iface_ != nullptr && this->direct_data_ != nullptr) {
      ret = enabled ? this->direct_iface_->enable_aec(this->direct_data_)
                    : this->direct_iface_->disable_aec(this->direct_data_);
      backend = "ESP-SR direct AFE";
    } else
#endif
#ifdef USE_ESP_AFE_GMF_PATH
        if (this->afe_manager_ != nullptr) {
      ret = static_cast<int>(esp_gmf_afe_manager_enable_features(this->afe_manager_, ESP_AFE_FEATURE_AEC, enabled));
      backend = "GMF AFE manager";
    } else
#endif
    {
      ESP_LOGW(TAG, "AFE %s AEC requested without a compiled backend", enabled ? "enable" : "disable");
      return false;
    }
    if (ret < 0) {
      ESP_LOGW(TAG, "AFE %s AEC failed (ret=%d)", enabled ? "enable" : "disable", ret);
      return false;
    }

    ESP_LOGI(TAG, "AEC %s via %s", enabled ? "enabled" : "disabled", backend);
    this->aec_enabled_.store(enabled, std::memory_order_relaxed);

    // Live toggle left AFE running. If the user just turned AEC off and
    // every other feature was already off, we should stop the whole
    // pipeline. Compute the decision while holding the lock so the
    // feature flags don't tear under a concurrent toggle.
    needs_rebuild = (!enabled && this->all_features_disabled_());
  }

  if (needs_rebuild) {
    this->recreate_instance_(false);  // no-features path tears down inside
  }
  return true;
}

bool EspAfe::set_vad_enabled_runtime_(bool enabled) {
  if (this->vad_enabled_.load(std::memory_order_relaxed) == enabled) {
    return true;
  }
  if (this->config_mutex_ == nullptr) {
    ESP_LOGW(TAG, "VAD toggle requested before setup");
    return false;
  }

  // AFE currently torn down (all features were off). Flip the flag and let
  // recreate_instance_ rebuild (or stay stopped if still all-off).
  if (this->afe_stopped_.load(std::memory_order_acquire)) {
    this->vad_enabled_.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
      this->voice_present_.store(false, std::memory_order_relaxed);
      return true;
    }
    return this->recreate_instance_(false);
  }

  if (!this->is_initialized()) {
    ESP_LOGW(TAG, "VAD toggle requested before initialization");
    return false;
  }

  bool needs_rebuild = false;
  {
    esp_audio_stack::ScopedLock lock(this->config_mutex_, CONFIG_MUTEX_TIMEOUT);
    if (!lock) {
      ESP_LOGW(TAG, "Timed out waiting to toggle VAD");
      return false;
    }

    int ret = 0;
    const char *backend = "unknown";
#ifdef USE_ESP_AFE_DIRECT_PATH
    if (this->direct_iface_ != nullptr && this->direct_data_ != nullptr) {
      ret = enabled ? this->direct_iface_->enable_vad(this->direct_data_)
                    : this->direct_iface_->disable_vad(this->direct_data_);
      if (ret >= 0) {
        this->direct_iface_->reset_vad(this->direct_data_);
      }
      backend = "ESP-SR direct AFE";
    } else
#endif
#ifdef USE_ESP_AFE_GMF_PATH
        if (this->afe_manager_ != nullptr) {
      ret = static_cast<int>(esp_gmf_afe_manager_enable_features(this->afe_manager_, ESP_AFE_FEATURE_VAD, enabled));
      backend = "GMF AFE manager";
    } else
#endif
    {
      ESP_LOGW(TAG, "AFE %s VAD requested without a compiled backend", enabled ? "enable" : "disable");
      return false;
    }
    if (ret < 0) {
      ESP_LOGW(TAG, "AFE %s VAD failed (ret=%d)", enabled ? "enable" : "disable", ret);
      return false;
    }

    ESP_LOGI(TAG, "VAD %s via %s", enabled ? "enabled" : "disabled", backend);
    this->vad_enabled_.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
      this->voice_present_.store(false, std::memory_order_relaxed);
    }
    needs_rebuild = (!enabled && this->all_features_disabled_());
  }

  if (needs_rebuild) {
    this->recreate_instance_(false);  // no-features path tears down inside
  }
  return true;
}

bool EspAfe::set_reinit_flag_(std::atomic<bool> &flag, bool enabled, const char *name) {
  if (flag.load(std::memory_order_relaxed) == enabled) {
    return true;
  }
  // ESP-SR can legitimately change the feed/fetch quantum when NS/AGC are
  // rebuilt. Accept that public contract and let frame_spec_revision_ make the
  // duplex task restart with the new shape.
  constexpr bool allow_frame_change = true;
  // AFE torn down (all-off) and a feature is coming back: commit the flag and
  // rebuild via recreate_instance_.
  if (this->afe_stopped_.load(std::memory_order_acquire)) {
    flag.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
      return true;  // stays torn down
    }
    return this->recreate_instance_(false);
  }
  if (!this->is_initialized() || this->config_mutex_ == nullptr || this->feed_chunksize_ == 0 ||
      this->fetch_chunksize_ == 0) {
    // Pre-activation after setup should already be prepared. If this branch is
    // reached, the instance exists but runtime preparation failed or was torn
    // down; rebuild so frame_spec() and the concrete instance stay in the same
    // MR/MMR shape.
    bool has_instance = false;
#ifdef USE_ESP_AFE_DIRECT_PATH
    has_instance = has_instance || this->direct_data_ != nullptr;
#endif
#ifdef USE_ESP_AFE_GMF_PATH
    has_instance = has_instance || this->afe_manager_ != nullptr;
#endif
    if (has_instance && this->config_mutex_ != nullptr && this->feed_chunksize_ > 0 && this->fetch_chunksize_ > 0) {
      bool old_value = flag.load(std::memory_order_relaxed);
      flag.store(enabled, std::memory_order_relaxed);
      ESP_LOGI(TAG, "Applying %s=%s (pre-activation rebuild, frame_size_change=%s)", name, enabled ? "true" : "false",
               allow_frame_change ? "allowed" : "locked");
      if (this->recreate_instance_(!allow_frame_change)) {
        return true;
      }
      ESP_LOGW(TAG, "Failed to apply %s=%s before activation, rolling back", name, enabled ? "true" : "false");
      flag.store(old_value, std::memory_order_relaxed);
      if (!this->recreate_instance_(!allow_frame_change)) {
        ESP_LOGE(TAG, "Rollback also failed for %s, AFE is down", name);
      }
      return false;
    }
    // Before setup: commit immediately, build_instance_ will use it at setup.
    flag.store(enabled, std::memory_order_relaxed);
    ESP_LOGD(TAG, "Deferring %s=%s until AFE is initialized", name, enabled ? "true" : "false");
    return true;
  }
  // Staged config: set flag, rebuild, rollback on failure.
  // Flag must be set before rebuild because build_instance_ reads it.
  // The mutex in recreate_instance_ ensures process() either sees the old
  // instance or the new one, never a mix.
  //
  bool old_value = flag.load(std::memory_order_relaxed);
  flag.store(enabled, std::memory_order_relaxed);
  ESP_LOGI(TAG, "Applying %s=%s (rebuild, frame_size_change=%s)", name, enabled ? "true" : "false",
           allow_frame_change ? "allowed" : "locked");
  if (this->recreate_instance_(!allow_frame_change)) {
    return true;
  }
  // Rebuild failed: restore flag and try to rebuild with the previous config.
  ESP_LOGW(TAG, "Failed to apply %s=%s, rolling back", name, enabled ? "true" : "false");
  flag.store(old_value, std::memory_order_relaxed);
  if (!this->recreate_instance_(!allow_frame_change)) {
    ESP_LOGE(TAG, "Rollback also failed for %s, AFE is down", name);
  }
  return false;
}

void EspAfe::setup() {
  if (!this->recreate_instance_(false)) {
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "AFE setup complete, runtime prepared and idle (waiting for mic consumer)");
}

void EspAfe::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP AFE (Audio Front End):");
  const char *type_name = (this->afe_type_ == AFE_TYPE_SR) ? "SR" : (this->afe_type_ == 3) ? "FD" : "VC";
  ESP_LOGCONFIG(TAG, "  Type: %s", type_name);
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->afe_mode_ == AFE_MODE_LOW_COST ? "LOW_COST" : "HIGH_PERF");
  ESP_LOGCONFIG(TAG, "  Microphones: transport=%d, afe=%d", this->mic_num_, this->afe_mic_channels_());
  ESP_LOGCONFIG(TAG, "  AEC: %s (filter_length=%d, nlp=%s)",
                this->aec_enabled_.load(std::memory_order_relaxed) ? "ON" : "OFF", this->aec_filter_length_,
                aec_nlp_level_name(this->aec_nlp_level_));
  ESP_LOGCONFIG(TAG, "  AEC scratch: ~12 KB internal (always allocated, "
                     "live-toggle via esp-sr vtable; off-at-boot still pays the cost)");
  ESP_LOGCONFIG(TAG, "  NS: %s (WebRTC)", this->ns_enabled_.load(std::memory_order_relaxed) ? "ON" : "OFF");
  ESP_LOGCONFIG(TAG, "  VAD: %s (mode=%d, speech=%dms, noise=%dms, delay=%dms)",
                this->vad_enabled_.load(std::memory_order_relaxed) ? "ON" : "OFF", this->vad_mode_,
                this->vad_min_speech_ms_, this->vad_min_noise_ms_, this->vad_delay_ms_);
  ESP_LOGCONFIG(TAG, "  Continuous VAD Background Input: %s", this->continuous_vad_ ? "ON" : "OFF");
  ESP_LOGCONFIG(TAG, "  AGC: %s (gain=%ddB, target=-%ddBFS)",
                this->agc_enabled_.load(std::memory_order_relaxed) ? "ON" : "OFF", this->agc_compression_gain_,
                this->agc_target_level_);
  if (this->mic_num_ >= 2) {
    ESP_LOGCONFIG(TAG, "  Speech Enhancement: %s", this->is_se_enabled() ? "ON (structural dual-mic)" : "OFF");
  } else {
    ESP_LOGCONFIG(TAG, "  Speech Enhancement: unavailable (mic_num < 2)");
  }
  ESP_LOGCONFIG(TAG, "  Input format override: %s",
                this->input_format_override_[0] ? this->input_format_override_ : "auto");
  if (this->mic_num_ >= 2) {
    ESP_LOGCONFIG(TAG, "  AEC-off dual-mic fallback: unavailable on official GMF element path");
  }
  ESP_LOGCONFIG(TAG, "  Alloc: %s, linear_gain=%.2f", this->memory_alloc_mode_to_str_(), this->afe_linear_gain_);
  ESP_LOGCONFIG(TAG, "  SE Task: core=%d, priority=%d, ringbuf=%d", this->task_core_, this->task_priority_,
                this->ringbuf_size_);
  ESP_LOGCONFIG(TAG, "  GMF Manager: feed core=%d prio=%d stack=%d, fetch core=%d prio=%d stack=%d",
                this->feed_task_core_, this->feed_task_priority_, this->feed_task_stack_size_, this->fetch_task_core_,
                this->fetch_task_priority_, this->fetch_task_stack_size_);
  ESP_LOGCONFIG(TAG, "  Process: %d samples, Feed: %d samples, Fetch: %d samples, Channels: %d",
                this->process_chunksize_, this->feed_chunksize_, this->fetch_chunksize_, this->total_channels_);
  ESP_LOGCONFIG(TAG, "  Initialized: %s", this->is_initialized() ? "YES" : "NO");
}

FrameSpec EspAfe::frame_spec() const {
  FrameSpec spec;
  spec.sample_rate = 16000;
  spec.mic_channels = this->afe_mic_channels_();
  spec.ref_channels = 1;
  // While running, use the live instance sizes. While torn down (all-off) the
  // live values are zero; fall back to the last successfully-installed spec
  // so consumers keep a stable frame shape while output is silenced.
  int live_in = this->process_chunksize_ > 0 ? this->process_chunksize_ : this->fetch_chunksize_;
  int live_out = this->fetch_chunksize_;
  spec.input_samples = live_in > 0 ? live_in : this->last_spec_process_size_;
  spec.output_samples = live_out > 0 ? live_out : this->last_spec_fetch_size_;
  return spec;
}

FeatureControl EspAfe::feature_control(AudioFeature feature) const {
  switch (feature) {
    case AudioFeature::AEC:
      return this->mic_num_ <= 1 ? FeatureControl::RESTART_REQUIRED : FeatureControl::LIVE_TOGGLE;
    case AudioFeature::VAD:
      return FeatureControl::RESTART_REQUIRED;
    case AudioFeature::NS:
    case AudioFeature::AGC:
      return FeatureControl::RESTART_REQUIRED;
    case AudioFeature::SE:
      return this->mic_num_ >= 2 ? FeatureControl::BOOT_ONLY : FeatureControl::NOT_SUPPORTED;
    default:
      return FeatureControl::NOT_SUPPORTED;
  }
}

bool EspAfe::set_feature(AudioFeature feature, bool enabled) {
  switch (feature) {
    case AudioFeature::AEC:
      return enabled ? this->enable_aec() : this->disable_aec();
    case AudioFeature::SE:
      return false;
    case AudioFeature::NS:
      return enabled ? this->enable_ns() : this->disable_ns();
    case AudioFeature::VAD:
      return enabled ? this->enable_vad() : this->disable_vad();
    case AudioFeature::AGC:
      return enabled ? this->enable_agc() : this->disable_agc();
    default:
      return false;
  }
}

ProcessorTelemetry EspAfe::telemetry() const {
  ProcessorTelemetry t;
  t.voice_present = this->voice_present_.load(std::memory_order_relaxed);
  t.input_volume_dbfs = this->input_volume_dbfs_.load(std::memory_order_relaxed);
  t.output_rms_dbfs = this->output_rms_dbfs_.load(std::memory_order_relaxed);
  t.ringbuf_free_pct = this->ringbuf_free_pct_.load(std::memory_order_relaxed);
  t.input_ring_drop = this->input_ring_drop_.load(std::memory_order_relaxed);
  t.feed_ok = this->feed_ok_.load(std::memory_order_relaxed);
  t.feed_rejected = this->feed_rejected_.load(std::memory_order_relaxed);
  t.fetch_ok = this->fetch_ok_.load(std::memory_order_relaxed);
  t.fetch_timeout = this->fetch_timeout_.load(std::memory_order_relaxed);
  t.output_ring_drop = this->output_ring_drop_.load(std::memory_order_relaxed);
  t.feed_queue_frames = this->feed_queue_frames_.load(std::memory_order_relaxed);
  t.feed_queue_peak = this->feed_queue_peak_.load(std::memory_order_relaxed);
  t.fetch_queue_frames = this->fetch_queue_frames_.load(std::memory_order_relaxed);
  t.fetch_queue_peak = this->fetch_queue_peak_.load(std::memory_order_relaxed);
  t.process_us_last = this->process_us_last_.load(std::memory_order_relaxed);
  t.process_us_max = this->process_us_max_.load(std::memory_order_relaxed);
  t.feed_us_last = this->feed_us_last_.load(std::memory_order_relaxed);
  t.feed_us_max = this->feed_us_max_.load(std::memory_order_relaxed);
  t.fetch_us_last = this->fetch_us_last_.load(std::memory_order_relaxed);
  t.fetch_us_max = this->fetch_us_max_.load(std::memory_order_relaxed);
  return t;
}

bool EspAfe::reconfigure(int type, int mode) {
  int old_type = this->afe_type_;
  int old_mode = this->afe_mode_;
  this->afe_type_ = type;
  this->afe_mode_ = mode;
  if (this->recreate_instance_(false)) {
    const char *type_name = (this->afe_type_ == AFE_TYPE_SR) ? "SR" : (this->afe_type_ == 3) ? "FD" : "VC";
    ESP_LOGI(TAG, "AFE reconfigured: type=%s, mode=%s", type_name,
             this->afe_mode_ == AFE_MODE_LOW_COST ? "LOW_COST" : "HIGH_PERF");
    return true;
  }
  // Rollback on failure: restore the previous config and rebuild to avoid leaving
  // the DSP permanently non-functional.
  ESP_LOGW(TAG, "reconfigure: new type=%d mode=%d build failed, rolling back to type=%d mode=%d", type, mode, old_type,
           old_mode);
  this->afe_type_ = old_type;
  this->afe_mode_ = old_mode;
  if (!this->recreate_instance_(false)) {
    ESP_LOGE(TAG, "reconfigure: rollback rebuild ALSO failed - AFE is DOWN");
  }
  return false;
}

bool EspAfe::process(const int16_t *in_mic, const int16_t *in_ref, int16_t *out, uint8_t mic_channels_in) {
  const int transport_mic_channels = std::max<int>(1, mic_channels_in);
  int qs = this->process_chunksize_ > 0 ? this->process_chunksize_ : this->fetch_chunksize_;
  int os = this->fetch_chunksize_;
  if (out == nullptr) {
    return false;
  }
  if (in_mic == nullptr) {
    if (os > 0) {
      memset(out, 0, static_cast<size_t>(os) * sizeof(int16_t));
    }
    return false;
  }
  // Fast path when user has disabled every AFE feature: the instance is torn
  // down, but the caller still expects a frame-shaped output. Emit silence
  // rather than leaking raw pre-AFE mic audio to MWW, VA or call components.
  if (this->afe_stopped_.load(std::memory_order_acquire)) {
    int pos = this->last_spec_fetch_size_ > 0 ? this->last_spec_fetch_size_ : os;
    silence_frame(out, pos);
    return false;
  }
  if (!this->is_initialized()) {
    silence_frame(out, os);
    return false;
  }

  const int64_t process_start_us = ESP_AFE_TIMING_TELEMETRY ? esp_timer_get_time() : 0;
  auto finish_process_timing = [this, process_start_us]() {
    if constexpr (ESP_AFE_TIMING_TELEMETRY) {
      uint32_t elapsed_us = static_cast<uint32_t>(std::max<int64_t>(0, esp_timer_get_time() - process_start_us));
      this->process_us_last_.store(elapsed_us, std::memory_order_relaxed);
      update_peak_atomic(this->process_us_max_, elapsed_us);
    }
  };

  // Seq_cst matches recreate_instance_ so either the writer sees busy=true, or
  // this frame sees drain_request_=true and emits silence before touching AFE.
  this->process_busy_.store(true, std::memory_order_seq_cst);
  if (this->drain_request_.load(std::memory_order_seq_cst)) {
    this->clear_process_busy_();
    silence_frame(out, os);
    finish_process_timing();
    return false;
  }

  const int afe_mic_channels = this->afe_mic_channels_();
  qs = this->process_chunksize_ > 0 ? this->process_chunksize_ : this->fetch_chunksize_;
  os = this->fetch_chunksize_;
  int fs = this->feed_chunksize_;
  bool direct_path = false;
#ifdef USE_ESP_AFE_DIRECT_PATH
  direct_path = this->direct_iface_ != nullptr && this->direct_data_ != nullptr;
#endif
  bool gmf_path = false;
#ifdef USE_ESP_AFE_GMF_PATH
  gmf_path = this->feed_input_ring_ != nullptr;
#endif
  if (qs <= 0 || os <= 0 || fs <= 0 || (!gmf_path && this->feed_buf_ == nullptr)) {
    silence_frame(out, os);
    this->clear_process_busy_();
    finish_process_timing();
    return false;
  }

  // Step 1: stage new input and feed it to AFE when a full frame is assembled.
  int offset = this->staged_input_samples_;
  if (offset + qs > fs) {
    ESP_LOGW(TAG, "AFE staging overflow (%d + %d > %d), dropping staged input", offset, qs, fs);
    offset = 0;
  }
  // Drop any partial frame staged with a different channel count: mixing
  // two layouts in feed_buf_ would feed the AFE garbage if a caller changes
  // the transport channel layout without recreating the instance first.
  if (this->last_process_mic_channels_ != 0 && this->last_process_mic_channels_ != transport_mic_channels &&
      offset > 0) {
    ESP_LOGD(TAG, "process(): mic_channels_in changed %d -> %d, resetting staged input",
             this->last_process_mic_channels_, transport_mic_channels);
    offset = 0;
  }
  this->last_process_mic_channels_ = transport_mic_channels;

  const bool sample_rms = (this->input_volume_sensor_enabled_ || this->output_rms_sensor_enabled_) &&
                          !this->warmup_remaining_ && qs > 0 && (++this->rms_sensor_divider_ >= 8);
  if (sample_rms) {
    this->rms_sensor_divider_ = 0;
  }

  // Input RMS: compute on the transport mic before feeding the AFE pipeline.
  // Replaces ESP-SR's app-level data_volume signal. Pass stride so we
  // read mic1 samples only when the transport delivers interleaved
  // channels (otherwise the RMS mixes mic1 + mic2 + reference and the
  // dBFS sensor reports nonsense).
  if (sample_rms && this->input_volume_sensor_enabled_) {
    this->input_volume_dbfs_.store(esp_audio_stack::compute_rms_dbfs_i16(in_mic, static_cast<size_t>(qs),
                                                                         static_cast<size_t>(transport_mic_channels)),
                                   std::memory_order_relaxed);
  }

  const int tc = this->total_channels_;
  const size_t feed_bytes = static_cast<size_t>(fs) * this->total_channels_ * sizeof(int16_t);
#ifdef USE_ESP_AFE_GMF_PATH
  const bool gmf_direct_frame = gmf_path && !direct_path && offset == 0 && qs == fs;
  void *gmf_slot = nullptr;
#endif
  bool staged = true;
#ifdef USE_ESP_AFE_GMF_PATH
  if (gmf_direct_frame) {
    gmf_slot = this->acquire_gmf_feed_slot_(feed_bytes, 0);
    if (gmf_slot == nullptr) {
      diag_add(this->input_ring_drop_);
      staged = false;
    } else {
      stage_afe_input_frame(static_cast<int16_t *>(gmf_slot), in_mic, in_ref, qs, transport_mic_channels,
                            afe_mic_channels, tc);
      offset = fs;
    }
  } else
#endif
      if (this->feed_buf_ != nullptr) {
    int16_t *dst = this->feed_buf_ + offset * tc;
    stage_afe_input_frame(dst, in_mic, in_ref, qs, transport_mic_channels, afe_mic_channels, tc);
    offset += qs;
  } else {
    diag_add(this->input_ring_drop_);
    staged = false;
  }

  if (staged && offset == fs) {
    if (this->warmup_remaining_ > 0) {
      this->warmup_remaining_--;
    }
#ifdef USE_ESP_AFE_DIRECT_PATH
    if (this->direct_iface_ != nullptr && this->direct_data_ != nullptr) {
      int ret = this->direct_iface_->feed(this->direct_data_, this->feed_buf_);
      if (ret > 0) {
        diag_add(this->feed_ok_);
        if (this->direct_feed_signal_ != nullptr && xSemaphoreGive(this->direct_feed_signal_) != pdTRUE) {
          diag_add(this->output_ring_drop_);
        }
      } else {
        diag_add(this->feed_rejected_);
      }
    } else
#endif
#ifdef USE_ESP_AFE_GMF_PATH
    {
      // Enqueue the full frame into the NOSPLIT ring. WS3-style GMF input uses
      // a direct ring slot, so the I2S task avoids staging and xRingbufferSend
      // copying. Fallback keeps partial-frame topologies on the old staging path.
      if (gmf_slot != nullptr) {
        if (!this->commit_gmf_feed_slot_(gmf_slot)) {
          diag_add(this->input_ring_drop_);
        } else {
          uint32_t queued = diag_increment_and_get(this->feed_queue_frames_);
          update_peak_atomic(this->feed_queue_peak_, queued);
        }
      } else if (this->feed_input_ring_ != nullptr && this->feed_buf_ != nullptr) {
        if (!xRingbufferSend(this->feed_input_ring_, this->feed_buf_, feed_bytes, 0)) {
          diag_add(this->input_ring_drop_);
        } else {
          uint32_t queued = diag_increment_and_get(this->feed_queue_frames_);
          update_peak_atomic(this->feed_queue_peak_, queued);
        }
      }
    }
#else
    { diag_add(this->feed_rejected_); }
#endif
    offset = 0;
  }
  this->staged_input_samples_ = offset;

  // Step 2: try to pull a processed frame that the fetch task has pushed into
  // our side of the bridge. Non-blocking: if nothing is ready we emit silence,
  // never raw pre-AFE mic. MWW, VA and call components must only see processed AFE
  // output while this component is active.
  size_t output_bytes = static_cast<size_t>(os) * sizeof(int16_t);
  bool processed = false;
  if (this->fetch_output_ring_) {
    size_t got = this->fetch_output_ring_->read(reinterpret_cast<uint8_t *>(out), output_bytes, 0);
    if (got == output_bytes) {
      processed = true;
      decrement_if_nonzero(this->fetch_queue_frames_);
      this->update_fetch_ring_free_pct_();
    }
  }
  if (!processed) {
    silence_frame(out, os);
  }

  // Release drain guard BEFORE emitting telemetry / counters: those touch
  // only atomics on this and can safely race with a rebuild.
  this->clear_process_busy_();

  // Output-side RMS depends on the samples handed to the caller. Input volume
  // is computed before feeding GMF, and VAD state comes from esp_gmf_afe events.
  if (processed && sample_rms && this->output_rms_sensor_enabled_) {
    this->output_rms_dbfs_.store(esp_audio_stack::compute_rms_dbfs_i16(out, os), std::memory_order_relaxed);
  }
  finish_process_timing();
  return processed;
}

bool EspAfe::reinit_by_name(const std::string &name) { return this->reinit_by_name(name.c_str()); }

bool EspAfe::reinit_by_name(const char *name) {
  if (name == nullptr) {
    ESP_LOGW(TAG, "Unknown AFE mode: (null)");
    return false;
  }
  for (const auto &preset : AFE_MODE_PRESETS) {
    if (std::strcmp(name, preset.name) == 0) {
      return this->reconfigure(preset.type, preset.mode);
    }
  }

  ESP_LOGW(TAG, "Unknown AFE mode: %s", name);
  return false;
}

bool EspAfe::start_reconfigure_task_() {
  if (this->reconfigure_queue_ == nullptr) {
    this->reconfigure_queue_ = xQueueCreateStatic(1, sizeof(ReconfigureRequest), this->reconfigure_queue_storage_,
                                                  &this->reconfigure_queue_struct_);
    if (this->reconfigure_queue_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create AFE reconfigure queue");
      return false;
    }
  }
  if (this->reconfigure_task_handle_ != nullptr) {
    return true;
  }

  this->reconfigure_task_running_.store(true, std::memory_order_release);
  if (!esp_audio_stack::start_pinned_task(
          &EspAfe::reconfigure_task_trampoline_, "afe_reinit", kReconfigureTaskStackBytes, this, 4, 0, false, TAG,
          &this->reconfigure_task_handle_, &this->reconfigure_task_tcb_, &this->reconfigure_task_stack_)) {
    this->reconfigure_task_running_.store(false, std::memory_order_release);
    return false;
  }
  return true;
}

bool EspAfe::request_reinit_by_name(const std::string &name) { return this->request_reinit_by_name(name.c_str()); }

bool EspAfe::request_reinit_by_name(const char *name) {
  if (name == nullptr || name[0] == '\0') {
    ESP_LOGW(TAG, "Cannot queue empty AFE mode");
    return false;
  }
  if (std::strcmp(name, this->get_mode_name().c_str()) == 0 &&
      !this->reconfigure_busy_.load(std::memory_order_acquire)) {
    this->last_reconfigure_ok_.store(true, std::memory_order_release);
    ESP_LOGI(TAG, "AFE already in mode %s", name);
    return true;
  }
  if (!this->start_reconfigure_task_()) {
    return false;
  }
  ReconfigureRequest req{};
  std::strncpy(req.mode, name, sizeof(req.mode) - 1);
  const bool already_pending =
      this->reconfigure_busy_.load(std::memory_order_acquire) || uxQueueMessagesWaiting(this->reconfigure_queue_) > 0;
  this->reconfigure_busy_.store(true, std::memory_order_release);
  this->last_reconfigure_ok_.store(false, std::memory_order_release);
  if (xQueueOverwrite(this->reconfigure_queue_, &req) != pdTRUE) {
    ESP_LOGW(TAG, "AFE reconfigure queue write failed; rejecting %s", name);
    this->reconfigure_busy_.store(false, std::memory_order_release);
    return false;
  }
  ESP_LOGI(TAG, "%s AFE reconfigure to %s", already_pending ? "Updated pending" : "Queued", req.mode);
  return true;
}

bool EspAfe::is_reconfigure_idle() const {
  if (this->reconfigure_queue_ == nullptr) {
    return !this->reconfigure_busy_.load(std::memory_order_acquire);
  }
  return !this->reconfigure_busy_.load(std::memory_order_acquire) &&
         uxQueueMessagesWaiting(this->reconfigure_queue_) == 0;
}

void EspAfe::reconfigure_task_trampoline_(void *arg) {
  static_cast<EspAfe *>(arg)->reconfigure_task_loop_();
  vTaskDelete(nullptr);
}

void EspAfe::reconfigure_task_loop_() {
  ReconfigureRequest req{};
  while (this->reconfigure_task_running_.load(std::memory_order_acquire)) {
    if (xQueueReceive(this->reconfigure_queue_, &req, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    this->reconfigure_busy_.store(true, std::memory_order_release);
    ESP_LOGI(TAG, "AFE async reconfigure begin: %s", req.mode);
    const bool ok = this->reinit_by_name(req.mode);
    this->last_reconfigure_ok_.store(ok, std::memory_order_release);
    this->reconfigure_busy_.store(false, std::memory_order_release);
    ESP_LOGI(TAG, "AFE async reconfigure %s: %s", ok ? "done" : "failed", req.mode);
  }
}

bool EspAfe::enable_aec() { return this->set_aec_enabled_runtime_(true); }
bool EspAfe::disable_aec() { return this->set_aec_enabled_runtime_(false); }
bool EspAfe::enable_ns() { return this->set_reinit_flag_(this->ns_enabled_, true, "ns_enabled"); }
bool EspAfe::disable_ns() { return this->set_reinit_flag_(this->ns_enabled_, false, "ns_enabled"); }
bool EspAfe::enable_vad() { return this->set_vad_enabled_runtime_(true); }
bool EspAfe::disable_vad() { return this->set_vad_enabled_runtime_(false); }
bool EspAfe::enable_agc() { return this->set_reinit_flag_(this->agc_enabled_, true, "agc_enabled"); }
bool EspAfe::disable_agc() { return this->set_reinit_flag_(this->agc_enabled_, false, "agc_enabled"); }

#ifdef USE_ESP_AFE_GMF_PATH
esp_gmf_err_io_t EspAfe::gmf_input_acquire_cb_(void *ctx, esp_gmf_payload_t *load, uint32_t wanted_size,
                                               int wait_ticks) {
  auto *self = static_cast<EspAfe *>(ctx);
  return self == nullptr ? ESP_GMF_IO_FAIL : self->gmf_input_acquire_(load, wanted_size, wait_ticks);
}

esp_gmf_err_io_t EspAfe::gmf_input_release_cb_(void *ctx, esp_gmf_payload_t *load, int wait_ticks) {
  (void) ctx;
  (void) load;
  (void) wait_ticks;
  return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t EspAfe::gmf_output_acquire_cb_(void *ctx, esp_gmf_payload_t *load, uint32_t wanted_size,
                                                int wait_ticks) {
  (void) ctx;
  (void) load;
  (void) wanted_size;
  (void) wait_ticks;
  return ESP_GMF_IO_OK;
}

esp_gmf_err_io_t EspAfe::gmf_output_release_cb_(void *ctx, esp_gmf_payload_t *load, int wait_ticks) {
  auto *self = static_cast<EspAfe *>(ctx);
  return self == nullptr ? ESP_GMF_IO_FAIL : self->gmf_output_release_(load, wait_ticks);
}

esp_gmf_err_io_t EspAfe::gmf_input_acquire_(esp_gmf_payload_t *load, uint32_t wanted_size, int wait_ticks) {
  if (load == nullptr || load->buf == nullptr || wanted_size == 0 || this->feed_input_ring_ == nullptr) {
    return ESP_GMF_IO_FAIL;
  }
  if (!this->processing_active_.load(std::memory_order_acquire)) {
    memset(load->buf, 0, wanted_size);
    load->valid_size = wanted_size;
    return ESP_GMF_IO_OK;
  }

  size_t item_size = 0;
  void *item = xRingbufferReceive(this->feed_input_ring_, &item_size, wait_ticks);
  if (item == nullptr) {
    diag_add(this->feed_rejected_);
    return ESP_GMF_IO_TIMEOUT;
  }

  decrement_if_nonzero(this->feed_queue_frames_);
  esp_gmf_err_io_t ret = ESP_GMF_IO_FAIL;
  if (item_size == static_cast<size_t>(wanted_size) && load->buf_length >= wanted_size) {
    const int64_t feed_start_us = ESP_AFE_TIMING_TELEMETRY ? esp_timer_get_time() : 0;
    memcpy(load->buf, item, item_size);
    load->valid_size = item_size;
    if constexpr (ESP_AFE_TIMING_TELEMETRY) {
      uint32_t feed_us = static_cast<uint32_t>(std::max<int64_t>(0, esp_timer_get_time() - feed_start_us));
      this->feed_us_last_.store(feed_us, std::memory_order_relaxed);
      update_peak_atomic(this->feed_us_max_, feed_us);
    }
    diag_add(this->feed_ok_);
    ret = ESP_GMF_IO_OK;
  } else {
    ESP_LOGW(TAG, "GMF AFE input size mismatch (%u != %u, buf=%u)", static_cast<unsigned>(item_size),
             static_cast<unsigned>(wanted_size), static_cast<unsigned>(load->buf_length));
    diag_add(this->feed_rejected_);
  }

  vRingbufferReturnItem(this->feed_input_ring_, item);
  return ret;
}

esp_gmf_err_io_t EspAfe::gmf_output_release_(esp_gmf_payload_t *load, int wait_ticks) {
  (void) wait_ticks;
  if (!this->processing_active_.load(std::memory_order_acquire)) {
    return ESP_GMF_IO_OK;
  }
  if (load == nullptr || load->buf == nullptr || load->valid_size == 0) {
    diag_add(this->fetch_timeout_);
    return ESP_GMF_IO_OK;
  }
  if (!this->fetch_output_ring_) {
    return ESP_GMF_IO_FAIL;
  }

  const size_t want = load->valid_size;
  size_t wrote = this->fetch_output_ring_->write_without_replacement(load->buf, want, 0, false);
  if (wrote != want) {
    diag_add(this->output_ring_drop_);
  } else {
    diag_add(this->fetch_ok_);
    uint32_t queued = diag_increment_and_get(this->fetch_queue_frames_);
    update_peak_atomic(this->fetch_queue_peak_, queued);
  }
  this->update_fetch_ring_free_pct_();
  TaskHandle_t waiter = this->pipeline_flush_waiter_.load(std::memory_order_acquire);
  if (waiter != nullptr) {
    xTaskNotifyGive(waiter);
  }
  return ESP_GMF_IO_OK;
}

#endif

void EspAfe::handle_manager_result_(afe_fetch_result_t *result) {
  if (!this->processing_active_.load(std::memory_order_acquire)) {
    return;
  }
  if (result == nullptr || result->data == nullptr || result->data_size <= 0) {
    diag_add(this->fetch_timeout_);
    return;
  }
  if (!this->fetch_output_ring_) {
    return;
  }

  const size_t want = static_cast<size_t>(result->data_size);
  const uint8_t *src_bytes = reinterpret_cast<const uint8_t *>(result->data);

  size_t wrote = this->fetch_output_ring_->write_without_replacement(src_bytes, want, 0, false);
  if (wrote != want) {
    diag_add(this->output_ring_drop_);
  } else {
    diag_add(this->fetch_ok_);
    uint32_t queued = diag_increment_and_get(this->fetch_queue_frames_);
    update_peak_atomic(this->fetch_queue_peak_, queued);
  }

  if (this->vad_enabled_.load(std::memory_order_relaxed)) {
    const bool new_voice = result->vad_state == VAD_SPEECH;
    const bool prev_voice = this->voice_present_.exchange(new_voice, std::memory_order_relaxed);
    if (new_voice != prev_voice) {
      ESP_LOGD(TAG, "AFE VAD transition: %s -> %s", prev_voice ? "speech" : "silence",
               new_voice ? "speech" : "silence");
    }
  }

  this->update_fetch_ring_free_pct_();
  TaskHandle_t waiter = this->pipeline_flush_waiter_.load(std::memory_order_acquire);
  if (waiter != nullptr) {
    xTaskNotifyGive(waiter);
  }
}

#ifdef USE_ESP_AFE_DIRECT_PATH
void EspAfe::direct_fetch_task_trampoline_(void *arg) {
  auto *self = static_cast<EspAfe *>(arg);
  self->direct_fetch_task_loop_();
  TaskHandle_t waiter = self->direct_fetch_stop_waiter_.load(std::memory_order_acquire);
  if (waiter != nullptr) {
    xTaskNotifyGive(waiter);
  }
  vTaskDelete(nullptr);
}

void EspAfe::direct_fetch_task_loop_() {
  const int feed_ms = (this->feed_chunksize_ > 0) ? (this->feed_chunksize_ / 16) : 32;
  const TickType_t fetch_timeout = pdMS_TO_TICKS(feed_ms + 10);
  while (this->direct_fetch_running_) {
    if (this->direct_feed_signal_ == nullptr || xSemaphoreTake(this->direct_feed_signal_, fetch_timeout) != pdTRUE) {
      continue;
    }
    if (!this->direct_fetch_running_ || this->direct_iface_ == nullptr || this->direct_data_ == nullptr) {
      break;
    }
    afe_fetch_result_t *result = this->direct_iface_->fetch_with_delay(this->direct_data_, fetch_timeout);
    if (!this->direct_fetch_running_) {
      break;
    }
    if (result == nullptr || result->ret_value != ESP_OK) {
      diag_add(this->fetch_timeout_);
      continue;
    }
    this->handle_manager_result_(result);
  }
}

bool EspAfe::start_direct_fetch_task_() {
  if (this->direct_fetch_task_handle_ != nullptr) {
    return true;
  }
  this->direct_fetch_stop_waiter_.store(nullptr, std::memory_order_release);
  if (this->direct_fetch_task_stack_ == nullptr) {
    this->direct_fetch_task_stack_ = static_cast<StackType_t *>(
        heap_caps_malloc(kDirectFetchTaskStackWords * sizeof(StackType_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (this->direct_fetch_task_stack_ == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate single-mic AFE fetch task stack");
      return false;
    }
  }
  this->direct_fetch_running_ = true;
  memset(&this->direct_fetch_task_tcb_, 0, sizeof(this->direct_fetch_task_tcb_));
  const int core = (this->task_core_ <= 0) ? 1 : (this->task_core_ >= 0 ? this->task_core_ : tskNO_AFFINITY);
  const int prio = this->task_priority_ > 1 ? this->task_priority_ - 1 : 1;
  this->direct_fetch_task_handle_ =
      xTaskCreateStaticPinnedToCore(&EspAfe::direct_fetch_task_trampoline_, "afe_fetch", kDirectFetchTaskStackWords,
                                    this, prio, this->direct_fetch_task_stack_, &this->direct_fetch_task_tcb_, core);
  if (this->direct_fetch_task_handle_ == nullptr) {
    this->direct_fetch_running_ = false;
    ESP_LOGE(TAG, "Failed to create single-mic AFE fetch task");
    return false;
  }
  ESP_LOGI(TAG, "Single-mic AFE fetch task started (core=%d, priority=%d)", core, prio);
  return true;
}

void EspAfe::stop_direct_fetch_task_() {
  if (this->direct_fetch_task_handle_ == nullptr) {
    this->direct_fetch_running_ = false;
    return;
  }
  TaskHandle_t waiter = xTaskGetCurrentTaskHandle();
  ulTaskNotifyTake(pdTRUE, 0);
  this->direct_fetch_stop_waiter_.store(waiter, std::memory_order_release);
  this->direct_fetch_running_ = false;
  if (this->direct_feed_signal_ != nullptr) {
    xSemaphoreGive(this->direct_feed_signal_);
  }
  if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250)) == 0) {
    ESP_LOGW(TAG, "Timed out waiting for single-mic AFE fetch task to exit");
  }
  // The fetch task notifies just before vTaskDelete(nullptr). Give the idle
  // task a short window to finish the deletion before this static TCB/stack is
  // reused by a rebuild requested from the main/API task.
  vTaskDelay(pdMS_TO_TICKS(10));
  this->direct_fetch_stop_waiter_.store(nullptr, std::memory_order_release);
  this->direct_fetch_task_handle_ = nullptr;
}
#endif

void EspAfe::update_fetch_ring_free_pct_() {
  if constexpr (ESP_AFE_DIAGNOSTIC_COUNTERS) {
    if (!this->fetch_output_ring_) {
      this->ringbuf_free_pct_.store(1.0f, std::memory_order_relaxed);
      return;
    }
    const size_t free_bytes = this->fetch_output_ring_->free();
    const size_t used_bytes = this->fetch_output_ring_->available();
    const size_t total_bytes = free_bytes + used_bytes;
    if (total_bytes > 0) {
      this->ringbuf_free_pct_.store(static_cast<float>(free_bytes) / static_cast<float>(total_bytes),
                                    std::memory_order_relaxed);
    }
  }
}

#ifdef USE_ESP_AFE_GMF_PATH
void EspAfe::gmf_event_cb_(esp_gmf_element_handle_t el, esp_gmf_afe_evt_t *event, void *user_data) {
  (void) el;
  auto *self = static_cast<EspAfe *>(user_data);
  if (self == nullptr || event == nullptr) {
    return;
  }

  bool new_voice = self->voice_present_.load(std::memory_order_relaxed);
  switch (event->type) {
    case ESP_GMF_AFE_EVT_VAD_START:
    case ESP_GMF_AFE_EVT_WAKEUP_START:
      new_voice = true;
      break;
    case ESP_GMF_AFE_EVT_VAD_END:
    case ESP_GMF_AFE_EVT_WAKEUP_END:
      new_voice = false;
      break;
    default:
      return;
  }
  const bool prev_voice = self->voice_present_.exchange(new_voice, std::memory_order_relaxed);
  if (new_voice != prev_voice) {
    ESP_LOGD(TAG, "GMF AFE voice transition: %s -> %s (event=%d)", prev_voice ? "speech" : "silence",
             new_voice ? "speech" : "silence", static_cast<int>(event->type));
  }
}
#endif

bool EspAfe::start_pipeline_() {
#ifdef USE_ESP_AFE_DIRECT_PATH
  if (this->direct_iface_ != nullptr && this->direct_data_ != nullptr) {
    if (this->afe_pipeline_running_) {
      return true;
    }
    this->staged_input_samples_ = 0;
    if (this->fetch_output_ring_) {
      this->fetch_output_ring_->reset();
    }
    this->feed_queue_frames_.store(0, std::memory_order_relaxed);
    this->fetch_queue_frames_.store(0, std::memory_order_relaxed);
    if (!this->start_direct_fetch_task_()) {
      return false;
    }
    this->afe_pipeline_running_ = true;
    this->afe_pipeline_paused_ = false;
    ESP_LOGI(TAG, "AFE active: ESP-SR direct single-mic feed/fetch");
    return true;
  }
#endif
#ifdef USE_ESP_AFE_GMF_PATH
  if (this->afe_pipeline_ == nullptr) {
    return false;
  }
  if (this->afe_pipeline_running_) {
    return true;
  }
  this->staged_input_samples_ = 0;
  this->drain_feed_input_ring_();
  if (this->fetch_output_ring_) {
    this->fetch_output_ring_->reset();
  }
  this->feed_queue_frames_.store(0, std::memory_order_relaxed);
  this->fetch_queue_frames_.store(0, std::memory_order_relaxed);
  if (this->afe_pipeline_paused_) {
    esp_gmf_err_t ret = esp_gmf_pipeline_resume(this->afe_pipeline_);
    if (ret != ESP_GMF_ERR_OK) {
      ESP_LOGW(TAG, "GMF AFE pipeline resume failed (ret=%d)", static_cast<int>(ret));
      if (this->afe_manager_ != nullptr) {
        esp_gmf_afe_manager_suspend(this->afe_manager_, true);
      }
      return false;
    }
    if (this->afe_manager_ != nullptr) {
      esp_gmf_afe_manager_suspend(this->afe_manager_, false);
    }
    this->afe_pipeline_paused_ = false;
    this->afe_pipeline_running_ = true;
    return true;
  }
  esp_gmf_err_t ret = esp_gmf_pipeline_run(this->afe_pipeline_);
  if (ret != ESP_GMF_ERR_OK) {
    ESP_LOGW(TAG, "GMF AFE pipeline run failed (ret=%d)", static_cast<int>(ret));
    if (this->afe_manager_ != nullptr) {
      esp_gmf_afe_manager_suspend(this->afe_manager_, true);
    }
    return false;
  }
  if (this->afe_manager_ != nullptr) {
    esp_gmf_afe_manager_suspend(this->afe_manager_, true);
  }
  if (this->afe_manager_ != nullptr) {
    esp_gmf_afe_manager_suspend(this->afe_manager_, false);
  }
  this->afe_pipeline_running_ = true;
  this->afe_pipeline_paused_ = false;
  return true;
#else
  return false;
#endif
}

bool EspAfe::pause_pipeline_() {
#ifdef USE_ESP_AFE_DIRECT_PATH
  if (this->direct_iface_ != nullptr && this->direct_data_ != nullptr) {
    this->stop_direct_fetch_task_();
    this->afe_pipeline_running_ = false;
    this->afe_pipeline_paused_ = true;
    return true;
  }
#endif
#ifdef USE_ESP_AFE_GMF_PATH
  if (this->afe_pipeline_ == nullptr) {
    if (this->afe_manager_ != nullptr) {
      esp_gmf_afe_manager_suspend(this->afe_manager_, true);
    }
    return true;
  }
  if (this->afe_pipeline_paused_) {
    if (this->afe_manager_ != nullptr) {
      esp_gmf_afe_manager_suspend(this->afe_manager_, true);
    }
    return true;
  }
  if (!this->afe_pipeline_running_) {
    if (this->afe_manager_ != nullptr) {
      esp_gmf_afe_manager_suspend(this->afe_manager_, true);
    }
    return true;
  }
  this->flush_pipeline_before_stop_();
  esp_gmf_err_t ret = esp_gmf_pipeline_pause(this->afe_pipeline_);
  if (ret != ESP_GMF_ERR_OK) {
    ESP_LOGW(TAG, "GMF AFE pipeline pause failed (ret=%d)", static_cast<int>(ret));
    return false;
  }
  this->afe_pipeline_running_ = false;
  this->afe_pipeline_paused_ = true;
  if (this->afe_manager_ != nullptr) {
    esp_gmf_afe_manager_suspend(this->afe_manager_, true);
  }
  return true;
#else
  this->afe_pipeline_running_ = false;
  this->afe_pipeline_paused_ = true;
  return true;
#endif
}

void EspAfe::stop_pipeline_() {
#ifdef USE_ESP_AFE_DIRECT_PATH
  if (this->direct_iface_ != nullptr && this->direct_data_ != nullptr) {
    this->stop_direct_fetch_task_();
    this->afe_pipeline_running_ = false;
    this->afe_pipeline_paused_ = false;
    if (this->fetch_output_ring_) {
      this->fetch_output_ring_->reset();
    }
    this->fetch_queue_frames_.store(0, std::memory_order_relaxed);
    return;
  }
#endif
#ifdef USE_ESP_AFE_GMF_PATH
  if (this->afe_pipeline_ != nullptr && (this->afe_pipeline_running_ || this->afe_pipeline_paused_)) {
    const bool was_paused = this->afe_pipeline_paused_;
    if (this->afe_pipeline_paused_ && this->afe_manager_ != nullptr) {
      esp_gmf_afe_manager_suspend(this->afe_manager_, false);
    }
    esp_gmf_err_t ret = esp_gmf_pipeline_stop(this->afe_pipeline_);
    ESP_LOGI(TAG, "GMF AFE pipeline stopped (was_paused=%d, ret=%d)", was_paused, static_cast<int>(ret));
    if (this->afe_manager_ != nullptr) {
      esp_gmf_afe_manager_suspend(this->afe_manager_, true);
    }
    this->afe_pipeline_running_ = false;
    this->afe_pipeline_paused_ = false;
  } else if (this->afe_manager_ != nullptr) {
    esp_gmf_afe_manager_suspend(this->afe_manager_, true);
  }
  this->drain_feed_input_ring_();
#else
  this->afe_pipeline_running_ = false;
  this->afe_pipeline_paused_ = false;
#endif
  if (this->fetch_output_ring_) {
    this->fetch_output_ring_->reset();
  }
  this->feed_queue_frames_.store(0, std::memory_order_relaxed);
  this->fetch_queue_frames_.store(0, std::memory_order_relaxed);
}

#ifdef USE_ESP_AFE_GMF_PATH
void EspAfe::flush_pipeline_before_stop_() {
  if (this->afe_manager_ == nullptr || this->feed_input_ring_ == nullptr || this->feed_chunksize_ <= 0 ||
      this->total_channels_ <= 0 || this->afe_stopped_.load(std::memory_order_acquire)) {
    return;
  }

  const size_t feed_bytes = static_cast<size_t>(this->feed_chunksize_) * this->total_channels_ * sizeof(int16_t);
  TaskHandle_t waiter = xTaskGetCurrentTaskHandle();
  ulTaskNotifyTake(pdTRUE, 0);
  this->pipeline_flush_waiter_.store(waiter, std::memory_order_release);
  void *slot = this->acquire_gmf_feed_slot_(feed_bytes, pdMS_TO_TICKS(10));
  if (slot != nullptr) {
    memset(slot, 0, feed_bytes);
  }
  if (slot != nullptr && this->commit_gmf_feed_slot_(slot)) {
    uint32_t queued = diag_increment_and_get(this->feed_queue_frames_);
    update_peak_atomic(this->feed_queue_peak_, queued);
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
  }
  this->pipeline_flush_waiter_.store(nullptr, std::memory_order_release);
}

void *EspAfe::acquire_gmf_feed_slot_(size_t feed_bytes, TickType_t ticks_to_wait) {
  if (this->feed_input_ring_ == nullptr || feed_bytes == 0) {
    return nullptr;
  }
  void *slot = nullptr;
  if (xRingbufferSendAcquire(this->feed_input_ring_, &slot, feed_bytes, ticks_to_wait) != pdTRUE) {
    return nullptr;
  }
  return slot;
}

bool EspAfe::commit_gmf_feed_slot_(void *slot) {
  if (this->feed_input_ring_ == nullptr || slot == nullptr) {
    return false;
  }
  return xRingbufferSendComplete(this->feed_input_ring_, slot) == pdTRUE;
}

bool EspAfe::prepare_feed_input_ring_() {
  if (this->afe_manager_ == nullptr || this->feed_chunksize_ <= 0) {
    return false;
  }

  if (this->feed_input_ring_ == nullptr) {
    // NOSPLIT ring: xRingbufferSend is atomic per-item; Receive returns a
    // complete frame. kBridgeRingFrames capacity absorbs BSS jitter without
    // memory bloat. Each NOSPLIT item adds an 8-byte header, included in sizing.
    const size_t frame_bytes = static_cast<size_t>(this->feed_chunksize_) * this->total_channels_ * sizeof(int16_t);
    const size_t ring_size = (frame_bytes + kRingbufferItemHeaderBytes) * kBridgeRingFrames;
    const uint32_t feed_ring_caps =
        this->feed_ring_in_psram_ ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    this->feed_input_ring_storage_ = static_cast<uint8_t *>(heap_caps_malloc(ring_size, feed_ring_caps));
    if (this->feed_input_ring_storage_ == nullptr && this->feed_ring_in_psram_) {
      ESP_LOGW(TAG, "feed_input_ring (%u bytes) fell back to internal RAM (PSRAM full/unavailable)",
               (unsigned) ring_size);
      this->feed_input_ring_storage_ =
          static_cast<uint8_t *>(heap_caps_malloc(ring_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (this->feed_input_ring_storage_ == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate AFE feed input ring storage (%u bytes)", (unsigned) ring_size);
      return false;
    }
    this->feed_input_ring_struct_ = static_cast<StaticRingbuffer_t *>(
        heap_caps_malloc(sizeof(StaticRingbuffer_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (this->feed_input_ring_struct_ == nullptr) {
      heap_caps_free(this->feed_input_ring_storage_);
      this->feed_input_ring_storage_ = nullptr;
      ESP_LOGE(TAG, "Failed to allocate AFE feed input ring struct");
      return false;
    }
    this->feed_input_ring_ = xRingbufferCreateStatic(ring_size, RINGBUF_TYPE_NOSPLIT, this->feed_input_ring_storage_,
                                                     this->feed_input_ring_struct_);
    if (this->feed_input_ring_ == nullptr) {
      heap_caps_free(this->feed_input_ring_storage_);
      this->feed_input_ring_storage_ = nullptr;
      heap_caps_free(this->feed_input_ring_struct_);
      this->feed_input_ring_struct_ = nullptr;
      ESP_LOGE(TAG, "Failed to create AFE feed input ring");
      return false;
    }
    ESP_LOGI(TAG, "Feed input ring: %u bytes (%u per frame, %u slots, NOSPLIT)", (unsigned) ring_size,
             (unsigned) frame_bytes, (unsigned) kBridgeRingFrames);
  }
  return true;
}
#endif

void EspAfe::set_processing_active(bool active) {
  if (active) {
    bool was = this->processing_active_.exchange(true, std::memory_order_acq_rel);
    if (was)
      return;
    if (this->afe_stopped_.load(std::memory_order_acquire)) {
      ESP_LOGI(TAG, "AFE active requested while stopped; pipeline will rebuild when a feature is enabled");
      return;
    }
    if (!this->start_pipeline_()) {
      ESP_LOGW(TAG, "AFE active: processing pipeline failed to run, staying idle");
      this->processing_active_.store(false, std::memory_order_release);
      return;
    }
    ESP_LOGI(TAG, "AFE active: processing pipeline running");
  } else {
    if (!this->processing_active_.exchange(false, std::memory_order_acq_rel))
      return;
    if (!this->afe_stopped_.load(std::memory_order_acquire)) {
      if (this->pause_pipeline_()) {
#ifdef USE_ESP_AFE_GMF_PATH
        this->drain_feed_input_ring_();
#endif
        if (this->fetch_output_ring_) {
          this->fetch_output_ring_->reset();
        }
        this->fetch_queue_frames_.store(0, std::memory_order_relaxed);
        ESP_LOGI(TAG, "AFE idle: processing pipeline suspended (no mic consumers)");
      } else {
#ifdef USE_ESP_AFE_GMF_PATH
        this->flush_pipeline_before_stop_();
#endif
        this->stop_pipeline_();
        ESP_LOGW(TAG, "AFE idle: processing pipeline stopped after pause failure");
      }
    } else {
      ESP_LOGI(TAG, "AFE idle: stopped pipeline remains torn down");
    }
  }
}

#ifdef USE_ESP_AFE_GMF_PATH
void EspAfe::drain_feed_input_ring_() {
  if (this->feed_input_ring_ == nullptr) {
    this->feed_queue_frames_.store(0, std::memory_order_relaxed);
    return;
  }
  uint32_t dropped = 0;
  while (true) {
    size_t item_size = 0;
    void *item = xRingbufferReceive(this->feed_input_ring_, &item_size, 0);
    if (item == nullptr) {
      break;
    }
    dropped++;
    vRingbufferReturnItem(this->feed_input_ring_, item);
  }
  this->feed_queue_frames_.store(0, std::memory_order_relaxed);
  if (dropped > 0) {
    diag_add(this->feed_rejected_, dropped);
    ESP_LOGD(TAG, "Dropped %u stale AFE feed frame(s) while stopping", (unsigned) dropped);
  }
}
#endif

bool EspAfe::prepare_fetch_output_ring_() {
  bool has_instance = false;
#ifdef USE_ESP_AFE_DIRECT_PATH
  has_instance = has_instance || this->direct_data_ != nullptr;
#endif
#ifdef USE_ESP_AFE_GMF_PATH
  has_instance = has_instance || this->afe_manager_ != nullptr;
#endif
  if (!has_instance || this->fetch_chunksize_ <= 0) {
    return false;
  }

  if (!this->fetch_output_ring_) {
    // NOSPLIT keeps each processed AFE frame atomic. process() either receives
    // a whole frame or emits silence; it never consumes a short byte-buffer read
    // that would shift the microphone surface seen by MWW/VA/call components.
    const size_t frame_bytes = static_cast<size_t>(this->fetch_chunksize_) * sizeof(int16_t);
    const size_t ring_bytes = (frame_bytes + kRingbufferItemHeaderBytes) * kBridgeRingFrames;
    this->fetch_output_ring_ =
        this->fetch_ring_in_psram_
            ? esp_audio_stack::create_nosplit_prefer_psram(ring_bytes, "esp_afe.fetch_output_ring")
            : esp_audio_stack::create_nosplit_internal(ring_bytes, "esp_afe.fetch_output_ring");
    if (!this->fetch_output_ring_) {
      ESP_LOGE(TAG, "Failed to allocate AFE fetch output ring buffer");
      return false;
    }
  }

  return true;
}

bool EspAfe::prepare_runtime_() {
  if (this->afe_stopped_.load(std::memory_order_acquire)) {
    return true;
  }
  this->log_memory_snapshot_("before_afe_prepare_runtime");
#ifdef USE_ESP_AFE_GMF_PATH
#ifdef USE_ESP_AFE_DIRECT_PATH
  if (this->direct_data_ == nullptr) {
    if (!this->prepare_feed_input_ring_()) {
      return false;
    }
  }
#else
  if (!this->prepare_feed_input_ring_()) {
    return false;
  }
#endif
#endif
  if (!this->prepare_fetch_output_ring_()) {
    return false;
  }
  this->log_memory_snapshot_("after_afe_prepare_runtime");
#ifdef USE_ESP_AFE_DIRECT_PATH
  const bool direct_path = this->direct_data_ != nullptr;
#else
  const bool direct_path = false;
#endif
  ESP_LOGI(TAG, "AFE runtime prepared (%s)", direct_path ? "ESP-SR direct single-mic" : "GMF feed/fetch rings");
  return true;
}

void EspAfe::release_runtime_buffers_() {
#ifdef USE_ESP_AFE_DIRECT_PATH
  this->stop_direct_fetch_task_();
  this->direct_feed_signal_ = nullptr;
#endif
  this->fetch_output_ring_.reset();
#ifdef USE_ESP_AFE_GMF_PATH
  this->feed_input_ring_ = nullptr;
  if (this->feed_input_ring_storage_ != nullptr) {
    heap_caps_free(this->feed_input_ring_storage_);
    this->feed_input_ring_storage_ = nullptr;
  }
  if (this->feed_input_ring_struct_ != nullptr) {
    heap_caps_free(this->feed_input_ring_struct_);
    this->feed_input_ring_struct_ = nullptr;
  }
#endif
}

EspAfe::~EspAfe() {
  // Quiesce: acquire mutex to ensure process() is not mid-frame.
  if (this->config_mutex_ != nullptr) {
    {
      esp_audio_stack::ScopedLock lock(this->config_mutex_, pdMS_TO_TICKS(500));
      AfeInstance instance = this->detach_instance_();
      this->destroy_instance_(&instance);
      this->release_runtime_buffers_();
    }
    vSemaphoreDelete(this->config_mutex_);
    this->config_mutex_ = nullptr;
  } else {
    AfeInstance instance = this->detach_instance_();
    this->destroy_instance_(&instance);
    this->release_runtime_buffers_();
  }
}

}  // namespace esp_afe
}  // namespace esphome

#endif  // USE_ESP32
