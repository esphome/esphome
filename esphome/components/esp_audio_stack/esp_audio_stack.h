#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "audio_core_audio_utils.h"
#include "audio_core_ring_buffer_caps.h"
#include "audio_core_scoped_lock.h"
#include "audio_core_task_utils.h"

#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
#include "codec_dev_backend.h"
#endif

#include <driver/i2s_std.h>
#if SOC_I2S_SUPPORTS_TDM && defined(USE_ESP_AUDIO_STACK_TDM_BUS)
#include <driver/i2s_tdm.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/semphr.h>

#include <atomic>
#include <climits>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "audio_core_processor.h"

namespace esphome::esp_audio_stack {

// Maximum listener count for microphone/speaker reference counting
static constexpr UBaseType_t MAX_LISTENERS = 16;

// Callback type for mic data: receives the public post-processor PCM stream
// (pointer + length, zero-copy). Raw/pre-processor taps are diagnostic-only and
// must not feed MWW, VA or call TX.
// IMPORTANT: Callbacks are invoked from the audio task (high priority, Core 0).
// They MUST NOT block, allocate memory, do network I/O, or hold locks.
// Target completion: <1ms to avoid I2S DMA underruns.
using MicDataCallback = void (*)(void *ctx, const uint8_t *data, size_t len);
// Callback type for speaker output: reports frames played and timestamp (for mixer pending_playback tracking).
// Same real-time constraints as MicDataCallback apply.
using SpeakerOutputCallback = void (*)(void *ctx, uint32_t frames, int64_t timestamp);

struct MicCallbackSlot {
  MicDataCallback fn{nullptr};
  void *ctx{nullptr};
};

struct SpeakerOutputCallbackSlot {
  SpeakerOutputCallback fn{nullptr};
  void *ctx{nullptr};
};

enum class AudioStackRuntimeState : uint8_t {
  IDLE = 0,
  MIC = 1,
  SPEAKER = 2,
  DUPLEX = 3,
};

enum class I2SHardwareState : uint8_t {
  UNPREPARED = 0,
  PREPARING = 1,
  READY = 2,
  RUNNING = 3,
  STOPPING = 4,
  ERROR = 5,
};

enum class RxPathKind : uint8_t {
  PASSTHROUGH = 0,
  MONO_EFFECTS = 1,
  STEREO_SLOT = 2,
  STEREO_REF = 3,
  TDM = 4,
};

static constexpr uint8_t MAX_RATE_CVT_CHANNELS = 3;

// Forward declarations for Pimpl.
class AudioEffectsRateConverterImpl;
class MultiChannelAudioEffectsRateConverterImpl;

#if defined(USE_ESP_AUDIO_STACK_MONO_RX) || defined(USE_ESP_AUDIO_STACK_MONO_REF)
// Sample-rate converter: consumes samples at high rate, produces at low rate.
// Backed by Espressif esp_ae_rate_cvt from esp_audio_effects.
class AudioEffectsRateConverter {
 public:
  AudioEffectsRateConverter();
  ~AudioEffectsRateConverter();
  AudioEffectsRateConverter(const AudioEffectsRateConverter &) = delete;
  AudioEffectsRateConverter &operator=(const AudioEffectsRateConverter &) = delete;

  void init(uint32_t ratio, uint32_t src_rate = 0, uint32_t dest_rate = 0, uint8_t complexity = 3,
            uint8_t perf_type = 1);
  void reset();

  // Contiguous int16 input.
  bool process(const int16_t *in, int16_t *out, size_t in_count);
  // Preallocate scratch and open the Espressif converter before the first realtime frame.
  bool prepare(size_t in_count, bool source_32bit = false);
  // Strided int16 input: deinterleave into scratch, then rate-convert.
  bool process_strided(const int16_t *in, int16_t *out, size_t out_count, size_t stride, size_t offset);
  // Strided int32 input with inline >>16 downshift.
  bool process_strided_32(const int32_t *in, int16_t *out, size_t out_count, size_t stride, size_t offset);

 private:
  std::unique_ptr<AudioEffectsRateConverterImpl> impl_;
};
#endif

#ifdef USE_ESP_AUDIO_STACK_MULTI_RX
// Multi-channel sample-rate converter: converts N channels from TDM/stereo
// rx_buffer in one pass. One esp_ae_rate_cvt handle processes all selected
// channels so mic/ref latency stays coupled. Max 3 channels (MMR: mic1 + mic2 + ref).
class MultiChannelAudioEffectsRateConverter {
 public:
  MultiChannelAudioEffectsRateConverter();
  ~MultiChannelAudioEffectsRateConverter();
  MultiChannelAudioEffectsRateConverter(const MultiChannelAudioEffectsRateConverter &) = delete;
  MultiChannelAudioEffectsRateConverter &operator=(const MultiChannelAudioEffectsRateConverter &) = delete;

  void init(uint32_t ratio, uint8_t num_channels, uint32_t src_rate = 0, uint32_t dest_rate = 0, uint8_t complexity = 3,
            uint8_t perf_type = 1);
  void reset();
  // Preallocate scratch and per-channel output buffers before the first
  // realtime TDM/stereo conversion frame.
  bool prepare(size_t in_count, size_t out_count, uint8_t num_channels, uint8_t source_channels, bool source_32bit);

  // Convert N channels from strided int16 TDM input, producing:
  //   - mic_interleaved: [mic1, mic2, mic1, mic2, ...] (num_mic_ch interleaved, for AFE)
  //   - mic_mono: mic1 contiguous (for callbacks/MWW/call components)
  //   - ref_out: ref contiguous (for AEC, may be nullptr if no ref channel)
  // channel_offsets: slot indices in TDM frame [mic1_slot, mic2_slot, ref_slot]
  // num_mic_ch: 1 or 2 (how many of the channels are mic, rest is ref)
  bool process_multi(const int16_t *in, size_t out_count, size_t in_stride, const uint8_t *channel_offsets,
                     int16_t *mic_interleaved, int16_t *mic_mono, int16_t *ref_out, uint8_t num_mic_ch);
  // Same but for 32-bit I2S input with inline >>16 downshift.
  bool process_multi_32(const int32_t *in, size_t out_count, size_t in_stride, const uint8_t *channel_offsets,
                        int16_t *mic_interleaved, int16_t *mic_mono, int16_t *ref_out, uint8_t num_mic_ch);

 private:
  std::unique_ptr<MultiChannelAudioEffectsRateConverterImpl> impl_;
};
#endif

/// Full-duplex I2S codec driver.
///
/// Extends the upstream i2s_audio pattern with a single shared I2S bus
/// that carries both mic RX and speaker TX (typical for codec chips
/// like ES8311 / ES7210). Optionally integrates an AudioProcessor (set
/// via processor_id) for AEC/NS/AGC/SE on the mic path. The processor
/// may be EspAec (AEC only) or EspAfe (full pipeline); both are drop-in
/// AudioProcessor implementations.
///
/// Runs a single audio task that pulls I2S RX frames, optionally
/// converts sample rate with AudioEffectsRateConverter, invokes the processor, and distributes
/// the result to registered MicDataCallback listeners. Speaker frames
/// come in via SpeakerOutputCallback and are written to I2S TX.
class ESPAudioStack : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  // PROCESSOR (=400) is the ESPHome tier for audio pipeline components;
  // HARDWARE (=800) is the I2C/SPI bus tier and runs too early. The
  // companion processors (esp_aec/esp_afe) also use PROCESSOR, so the
  // component-internal pointers (set_processor) are wired up before any
  // setup() touches the audio path.
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  // Pin setters
  void set_lrclk_pin(int pin) { this->lrclk_pin_ = pin; }
  void set_bclk_pin(int pin) { this->bclk_pin_ = pin; }
  void set_mclk_pin(int pin) { this->mclk_pin_ = pin; }
  void set_din_pin(int pin) { this->din_pin_ = pin; }
  void set_dout_pin(int pin) { this->dout_pin_ = pin; }
#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  void configure_rx_bus(uint8_t i2s_num, int lrclk_pin, int bclk_pin, int mclk_pin, int din_pin) {
    this->rx_bus_.configured = true;
    this->rx_bus_.i2s_num = i2s_num;
    this->rx_bus_.lrclk_pin = lrclk_pin;
    this->rx_bus_.bclk_pin = bclk_pin;
    this->rx_bus_.mclk_pin = mclk_pin;
    this->rx_bus_.din_pin = din_pin;
    this->dual_i2s_bus_ = true;
  }
  void configure_tx_bus(uint8_t i2s_num, int lrclk_pin, int bclk_pin, int mclk_pin, int dout_pin) {
    this->tx_bus_.configured = true;
    this->tx_bus_.i2s_num = i2s_num;
    this->tx_bus_.lrclk_pin = lrclk_pin;
    this->tx_bus_.bclk_pin = bclk_pin;
    this->tx_bus_.mclk_pin = mclk_pin;
    this->tx_bus_.dout_pin = dout_pin;
    this->dual_i2s_bus_ = true;
  }
#endif
  void set_sample_rate(uint32_t rate) { this->sample_rate_ = rate; }
  void set_output_sample_rate(uint32_t rate) { this->output_sample_rate_ = rate; }
  void set_bits_per_sample(uint8_t bps) { this->bits_per_sample_ = bps; }
  uint8_t get_bits_per_sample() const { return this->bits_per_sample_; }
  void set_correct_dc_offset(bool enabled) { this->correct_dc_offset_ = enabled; }
  void set_num_channels(uint8_t ch) { this->num_channels_ = ch; }
  uint8_t get_num_channels() const { return this->num_channels_; }
  void set_speaker_channels(uint8_t ch) { this->speaker_channels_ = ch; }
  uint8_t get_speaker_channels() const { return this->use_tdm_bus_ ? 1 : this->speaker_channels_; }
  void set_i2s_mode_secondary(bool secondary) { this->i2s_mode_secondary_ = secondary; }
  void set_use_apll(bool use) { this->use_apll_ = use; }
  void set_i2s_num(uint8_t num) { this->i2s_num_ = num; }
  void set_mclk_multiple(uint32_t mult) { this->mclk_multiple_ = mult; }
  void set_i2s_comm_fmt(uint8_t fmt) { this->i2s_comm_fmt_ = fmt; }
  void set_mic_channel_right(bool right) { this->mic_channel_right_ = right; }
  void set_rx_slot_mode_stereo(bool stereo) { this->rx_slot_mode_stereo_ = stereo; }
  void set_tx_slot_right(bool right) { this->tx_slot_right_ = right; }
  void set_slot_bit_width(uint8_t sbw) { this->slot_bit_width_ = sbw; }
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  void set_codec_i2c_bus(i2c::I2CBus *bus) { this->codec_backend_.set_i2c_bus(bus); }
  void configure_es7210_codec(uint8_t address, uint8_t mic_selected, float input_gain_db, bool has_ref_channel_gain,
                              uint8_t ref_channel, float ref_channel_gain_db) {
    CodecDevBackend::Es7210Config cfg;
    cfg.enabled = true;
    cfg.address = address;
    cfg.mic_selected = mic_selected;
    cfg.input_gain_db = input_gain_db;
    cfg.has_ref_channel_gain = has_ref_channel_gain;
    cfg.ref_channel = ref_channel;
    cfg.ref_channel_gain_db = ref_channel_gain_db;
    this->codec_backend_.set_es7210_config(cfg);
  }
  void configure_es8311_input_codec(uint8_t address, bool use_mclk, bool no_dac_ref, float input_gain_db) {
    CodecDevBackend::GenericCodecConfig cfg;
    cfg.enabled = true;
    cfg.kind = CodecDevBackend::CodecKind::ES8311;
    cfg.address = address;
    cfg.use_mclk = use_mclk;
    cfg.no_dac_ref = no_dac_ref;
    cfg.input_gain_db = input_gain_db;
    this->codec_backend_.set_input_codec_config(cfg);
  }
  void configure_input_codec(uint8_t kind, uint8_t address, bool use_mclk, bool no_dac_ref, float input_gain_db) {
    CodecDevBackend::GenericCodecConfig cfg;
    cfg.enabled = true;
    cfg.kind = static_cast<CodecDevBackend::CodecKind>(kind);
    cfg.address = address;
    cfg.use_mclk = use_mclk;
    cfg.no_dac_ref = no_dac_ref;
    cfg.input_gain_db = input_gain_db;
    this->codec_backend_.set_input_codec_config(cfg);
  }
  void configure_es8311_codec(uint8_t address, bool use_mclk, bool no_dac_ref) {
    CodecDevBackend::GenericCodecConfig cfg;
    cfg.enabled = true;
    cfg.kind = CodecDevBackend::CodecKind::ES8311;
    cfg.address = address;
    cfg.use_mclk = use_mclk;
    cfg.no_dac_ref = no_dac_ref;
    this->codec_backend_.set_output_codec_config(cfg);
  }
  void configure_output_codec(uint8_t kind, uint8_t address, bool use_mclk, bool no_dac_ref) {
    CodecDevBackend::GenericCodecConfig cfg;
    cfg.enabled = true;
    cfg.kind = static_cast<CodecDevBackend::CodecKind>(kind);
    cfg.address = address;
    cfg.use_mclk = use_mclk;
    cfg.no_dac_ref = no_dac_ref;
    this->codec_backend_.set_output_codec_config(cfg);
  }
#endif

  // Optional AEC/AFE processor attachment.
  void set_processor(AudioProcessor *processor);
  void set_processor_enabled(bool enabled) { this->processor_enabled_.store(enabled, std::memory_order_relaxed); }
  bool is_processor_enabled() const { return this->processor_enabled_.load(std::memory_order_relaxed); }

  // Mic gain control. Attenuation uses esp-audio-libs Q31 in the hot path;
  // positive dB boost uses Espressif esp_ae_alc because Q31 gain cannot
  // represent amplification above unity.
  void set_mic_gain(float gain);
  float get_mic_gain() const { return this->mic_gain_.load(std::memory_order_relaxed); }

  // Input gain before the audio processor, for hot or weak mics.
  void set_input_gain(float gain);
  float get_input_gain() const { return this->input_gain_.load(std::memory_order_relaxed); }
  // Master volume is independent from the speaker abstraction volume used by
  // ESPHome's media_player. The audio task applies media/speaker volume *
  // master volume, so HA media volume and the board master control cascade.
  void set_master_volume_min_db(float db);
  void set_master_volume(float volume);
  void set_master_volume_q31(int32_t q31);
  void set_output_volume(float volume);
  void set_output_volume_q31(int32_t q31);
  float get_master_volume() const { return this->master_volume_public_.load(std::memory_order_relaxed); }

  // ES8311 Digital Feedback mode: RX is stereo with L=ADC(mic), R=DAC(ref)
  void set_use_stereo_aec_reference(bool use) { this->use_stereo_aec_ref_ = use; }

  // Reference channel selection: false=left (default), true=right
  void set_reference_channel_right(bool right) { this->ref_channel_right_ = right; }

  // TDM bus/reference: TDM bus selects TDM RX/TX slot layout; TDM reference
  // controls whether the AEC ref comes from the configured TDM slot or from
  // the software speaker reference path.
  void set_use_tdm_bus(bool use) { this->use_tdm_bus_ = use; }
  void set_use_tdm_reference(bool use) { this->use_tdm_ref_ = use; }
  void set_tdm_total_slots(uint8_t n) { this->tdm_total_slots_ = n; }
  void set_tdm_mic_slot(uint8_t slot) { this->tdm_mic_slot_ = slot; }
  void set_secondary_tdm_mic_slot(int8_t slot) { this->tdm_second_mic_slot_ = slot; }
  void set_tdm_ref_slot(uint8_t slot) { this->tdm_ref_slot_ = slot; }
  void set_tdm_tx_slot(uint8_t slot) { this->tdm_tx_slot_ = slot; }
  void set_tdm_slot_level_sensor_enabled(uint8_t slot, bool enabled) {
    if (slot >= 8)
      return;
    this->tdm_slot_level_sensor_enabled_[slot] = enabled;
    bool any = false;
    for (bool slot_enabled : this->tdm_slot_level_sensor_enabled_) {
      any = any || slot_enabled;
    }
    this->any_tdm_slot_level_sensor_enabled_.store(any, std::memory_order_relaxed);
  }
  float get_tdm_slot_level_dbfs(uint8_t slot) const {
    if (slot >= 8)
      return -120.0f;
    return this->tdm_slot_level_dbfs_[slot].load(std::memory_order_relaxed);
  }

  // Microphone interface
  bool add_mic_data_callback(MicDataCallback callback, void *ctx) {
    if (callback == nullptr)
      return false;
    if (this->mic_callback_count_ >= MAX_LISTENERS)
      return false;
    this->mic_callbacks_[this->mic_callback_count_++] = MicCallbackSlot{callback, ctx};
    return true;
  }

  // Consumer registry: each consumer (a microphone wrapper, call TX path,
  // etc.) registers an opaque token. The audio task gates mic callbacks on
  // has_mic_consumers_. Registration survives an internal stop()+start()
  // sequence (e.g. frame_spec change), so consumers stay connected across
  // reconfigure without having to re-register. Idempotent per token.
  bool register_mic_consumer(void *token);
  void unregister_mic_consumer(void *token);
  bool is_mic_running() const { return this->has_mic_consumers_.load(std::memory_order_relaxed); }

  // Speaker interface: data arrives at bus rate (from mixer/resampler)
  size_t play(const uint8_t *data, size_t len, TickType_t ticks_to_wait = portMAX_DELAY);
  void start_speaker();
  void stop_speaker();
  void set_speaker_buffer_duration(uint32_t ms) { this->speaker_buffer_duration_ms_ = ms; }
  bool is_speaker_running() const { return this->speaker_running_.load(std::memory_order_relaxed); }
  void set_speaker_paused(bool paused) { this->speaker_paused_.store(paused, std::memory_order_relaxed); }
  bool is_speaker_paused() const { return this->speaker_paused_.load(std::memory_order_relaxed); }

  // Audio stack lifecycle control
  void start();  // Start both mic and speaker
  void stop();   // Stop both
  bool stop_and_wait(uint32_t timeout_ms = 2000);

  bool is_running() const { return this->audio_stack_running_.load(std::memory_order_relaxed); }
  bool is_idle() const {
    return !this->audio_stack_running_.load(std::memory_order_relaxed) &&
           this->audio_task_idle_.load(std::memory_order_relaxed) &&
           !this->teardown_pending_.load(std::memory_order_relaxed);
  }
  bool has_i2s_error() const { return this->has_i2s_error_.load(std::memory_order_relaxed); }
  Trigger<> *get_start_trigger() { return &this->start_trigger_; }
  Trigger<> *get_idle_trigger() { return &this->idle_trigger_; }
  Trigger<std::string> *get_state_trigger() { return &this->state_trigger_; }
  Trigger<> *get_mic_start_trigger() { return &this->mic_start_trigger_; }
  Trigger<> *get_mic_idle_trigger() { return &this->mic_idle_trigger_; }
  Trigger<> *get_speaker_start_trigger() { return &this->speaker_start_trigger_; }
  Trigger<> *get_speaker_idle_trigger() { return &this->speaker_idle_trigger_; }

  // Speaker output callback registration (for mixer pending_playback_frames tracking)
  bool add_speaker_output_callback(SpeakerOutputCallback callback, void *ctx) {
    if (callback == nullptr)
      return false;
    if (this->speaker_output_callback_count_ >= MAX_LISTENERS)
      return false;
    this->speaker_output_callbacks_[this->speaker_output_callback_count_++] = SpeakerOutputCallbackSlot{callback, ctx};
    return true;
  }

  // Getters for platform wrappers
  // get_sample_rate() returns the I2S bus rate (used by speaker for audio_stream_info)
  uint32_t get_sample_rate() const { return this->sample_rate_; }
  // get_output_sample_rate() returns the converted rate for mic consumers (MWW/AEC/VA/call components)
  uint32_t get_output_sample_rate() const {
    return this->output_sample_rate_ > 0 ? this->output_sample_rate_ : this->sample_rate_;
  }
  size_t get_mic_callback_buffer_size() const;
  size_t get_speaker_buffer_available() const;
  bool has_pending_speaker_output() const {
    return this->tx_completion_pending_real_records_.load(std::memory_order_acquire) > 0;
  }
  size_t get_speaker_buffer_size() const;

  // Task configuration (settable from YAML)
  void set_task_priority(uint8_t prio) { this->task_priority_ = prio; }
  void set_task_core(int8_t core) { this->task_core_ = core; }
  void set_task_stack_size(uint32_t size) { this->task_stack_size_ = size; }
  void set_dma_desc_num(uint32_t desc_num) { this->dma_desc_num_ = desc_num; }
  void set_dma_frame_num(uint32_t frame_num) {
    this->dma_frame_num_ = frame_num;
    this->dma_frame_num_configured_ = true;
  }
  void set_buffers_in_psram(bool psram) { this->buffers_in_psram_ = psram; }
  void set_audio_task_stack_in_psram(bool psram) { this->audio_task_stack_in_psram_ = psram; }
#ifdef USE_ESP_AUDIO_STACK_RING_REF
  void set_aec_ref_buffer_ms(uint32_t ms) { this->aec_ref_buffer_ms_ = ms; }
#else
  void set_aec_ref_buffer_ms(uint32_t ms) { (void) ms; }
#endif
#ifdef USE_ESP_AUDIO_STACK_RING_REF
  void set_aec_ref_ring_in_psram(bool psram) { this->aec_ref_ring_in_psram_ = psram; }
#else
  void set_aec_ref_ring_in_psram(bool psram) { (void) psram; }
#endif
  void set_telemetry_log_interval_frames(uint16_t frames) { this->telemetry_log_interval_frames_ = frames; }
  void set_rate_cvt_complexity(uint8_t complexity) { this->rate_cvt_complexity_ = complexity; }
  void set_rate_cvt_perf_type(uint8_t perf_type) { this->rate_cvt_perf_type_ = perf_type; }

 protected:
  bool init_audio_stack_();
  bool prepare_i2s_channels_();
  bool enable_i2s_channels_();
  void close_audio_io_();
  void deinit_i2s_();
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  bool setup_codec_backend_(i2s_clock_src_t clk_src);
  CodecDevBackend::SampleConfig make_tx_sample_config_() const;
  CodecDevBackend::SampleConfig make_rx_sample_config_() const;
#endif

  static void audio_task(void *param);
  // Top-level task entry: outer loop that spawns one audio_session_ per start()/stop()
  // cycle. Created once in setup(), never destroyed, so subsequent cycles can't trip
  // a FreeRTOS xTaskCreate race on a lingering TCB.
  void audio_task_();
  // One audio session: populate ctx, enter the processing loop, and return when
  // stop() flips audio_stack_running_ off or the processor's frame_spec changes.
  void audio_session_();
  void update_hot_output_volume_();

  // Audio task context: groups all buffers, sizes, and per-frame snapshots
  // to avoid long parameter lists in the refactored processing functions.
  struct AudioTaskCtx {
    // ── Invariants (set once at task start) ──
    uint32_t ratio{1};
    uint8_t i2s_bps{2};  // 2 or 4 bytes per I2S sample
    uint8_t num_ch{1};   // TX channels
    bool use_stereo_aec_ref{false};
    bool rx_slot_mode_stereo{false};
    bool use_tdm_bus{false};
    bool use_tdm_ref{false};
    bool ref_channel_right{false};
    bool correct_dc_offset{false};
    uint8_t tdm_total_slots{0};
    uint8_t tdm_mic_slot{0};
    int8_t tdm_second_mic_slot{-1};
    uint8_t tdm_ref_slot{0};
    uint8_t tdm_tx_slot{0};
    uint8_t speaker_channels{1};
    uint8_t processor_mic_channels{1};
    uint32_t processor_spec_revision{0};
    bool processor_spec_loaded{false};
    RxPathKind rx_path_kind{RxPathKind::PASSTHROUGH};
    uint8_t rx_rate_converter_channels{0};
    bool tdm_ref_active_monitor_logged{false};

    // ── Frame sizing ──
    size_t input_frame_size{0};
    size_t output_frame_size{0};
    size_t bus_frame_size{0};
    size_t input_frame_bytes{0};
    size_t output_frame_bytes{0};
    size_t bus_frame_bytes{0};
    size_t speaker_frame_bytes{0};
    size_t rx_frame_bytes{0};
    size_t tdm_tx_frame_bytes{0};

    // ── Working buffers (heap-allocated, owned by audio_task_) ──
    // processor_mic_buffer is interleaved when a secondary mic is active
    // (layout: [mic1[i], mic2[i]] for i in [0, rx_frames)).
    // Rate conversion reads rx_buffer with a stride, so no separate
    // deinterleave buffer is kept.
    int16_t *rx_buffer{nullptr};
    int16_t *mic_buffer{nullptr};
    int16_t *processor_mic_buffer{nullptr};
    int16_t *spk_buffer{nullptr};
    int16_t *spk_ref_buffer{nullptr};
    int16_t *tx_ref_mono_buffer{nullptr};
    int16_t *tdm_tx_buffer{nullptr};
    int16_t *tx_interleave_buffer{nullptr};
    int16_t *tx_silence_buffer{nullptr};
    int16_t *tx_clock_buffer{nullptr};
    int16_t *tx_32_buffer{nullptr};
    int16_t *aec_output{nullptr};

    // ── Loop mutable state ──
    int consecutive_i2s_errors{0};
    // INVALID_STATE counter: codec/GMF IO can surface invalid state
    // briefly when stop() disables a channel; counted separately so we
    // can escalate only when it persists while audio_stack_running_ stays true
    // (channel corrupted independently of our own teardown).
    int invalid_state_errors{0};
    // DC-HPF state lives on the component; the process_rx_path_ loop keeps
    // converging across audio_session_ restarts (frame_spec changes
    // would otherwise zero the state and produce ~10 ms of mic gain
    // transient on every reconfigure).
    const int16_t *processor_input{nullptr};
    int16_t *output_buffer{nullptr};  // points to mic_buffer or aec_output
    size_t current_output_frame_bytes{0};
    size_t current_output_frame_size{0};
    bool mic_separate{false};  // true if mic_buffer != rx_buffer
    bool need_rx_processing{false};
    bool need_rx_drain{false};
    bool need_tx_audio{false};
    bool need_tx_clock{false};
    bool clock_only_tx{false};
    bool any_tdm_slot_level_sensor_enabled{false};

    // ── Per-iteration snapshots from atomics ──
    int8_t mic_gain_boost_db{0};
    int32_t mic_gain_q31{INT32_MAX};
    float input_gain_boost{1.0f};
    int32_t input_gain_q31{INT32_MAX};
    int32_t hot_output_volume_q31{INT32_MAX};
    bool processor_enabled{false};
    bool processor_ready{false};  // cached: enabled && initialized (avoids virtual call per frame)
    bool speaker_running{false};
    bool speaker_paused{false};
    bool speaker_underrun{false};
#ifdef USE_ESP_AUDIO_STACK_TDM_REF_DIAGNOSTIC
    size_t previous_speaker_got{0};  // previous TX frame, used to validate captured full-duplex ref
    float previous_speaker_dbfs{-120.0f};
    float current_speaker_dbfs{-120.0f};
#endif
    size_t speaker_got{0};  // bytes actually read from speaker ring buffer
    bool mic_running{false};
    uint32_t now_ms{0};
  };

  // Refactored audio processing functions (called from audio_task_ main loop)
  void update_runtime_audio_flags_(AudioTaskCtx &ctx);
  void process_rx_path_(AudioTaskCtx &ctx);
  bool read_rx_frame_(AudioTaskCtx &ctx, const char *label);
  void fail_audio_session_(const char *stage);
  bool convert_rx_frame_(AudioTaskCtx &ctx);
  bool process_rx_passthrough_(AudioTaskCtx &ctx);
  bool process_rx_mono_effects_(AudioTaskCtx &ctx);
  bool process_rx_stereo_slot_(AudioTaskCtx &ctx);
  bool process_rx_stereo_ref_(AudioTaskCtx &ctx);
  bool process_rx_tdm_(AudioTaskCtx &ctx);
  void apply_input_conditioning_(AudioTaskCtx &ctx);
  void drain_rx_minimal_(AudioTaskCtx &ctx);
  void process_aec_and_callbacks_(AudioTaskCtx &ctx);
  void process_tx_path_(AudioTaskCtx &ctx);
  void process_tx_clock_only_(AudioTaskCtx &ctx);
  bool write_tx_frame_(AudioTaskCtx &ctx, void *tx_data, size_t tx_bytes);
  bool write_tx_dma_blocks_(AudioTaskCtx &ctx, void *tx_data, size_t tx_bytes, size_t real_speaker_bytes);
  bool can_dispatch_mic_frame_(const AudioTaskCtx &ctx) const;
  void apply_post_processor_gain_(AudioTaskCtx &ctx);
  void dispatch_mic_callbacks_(const AudioTaskCtx &ctx);
  bool apply_mic_alc_gain_(int16_t *samples, size_t sample_count, int8_t gain_db);
#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
  void update_tdm_slot_levels_(const AudioTaskCtx &ctx);
#endif
  bool format_tx_frame_(AudioTaskCtx &ctx, void **tx_data, size_t *tx_bytes);
#ifdef USE_ESP_AUDIO_STACK_32BIT
  bool tx_bit_cvt_16_to_32_(uint8_t channels, const void *in, uint32_t sample_num, void *out);
#endif
#ifdef USE_AUDIO_PROCESSOR
  void run_processor_(AudioTaskCtx &ctx);
#endif
  bool wait_audio_task_state_(bool idle, uint32_t timeout_ms);
  bool wait_audio_task_idle_(uint32_t timeout_ms) { return this->wait_audio_task_state_(true, timeout_ms); }
  bool wait_audio_task_active_(uint32_t timeout_ms) { return this->wait_audio_task_state_(false, timeout_ms); }

  struct TxCompletionRecord {
    uint32_t real_frames{0};
    uint32_t trailing_silence_frames{0};
  };
  static bool IRAM_ATTR tx_on_sent_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);
  bool prepare_tx_completion_tracking_();
  bool prime_tx_completion_records_(bool preload_dma);
  void drain_tx_completion_events_();
  void reset_tx_completion_tracking_();
  bool queue_tx_completion_record_(const TxCompletionRecord &record);
  void dispatch_speaker_output_callbacks_(uint32_t frames, int64_t timestamp);

#ifdef USE_ESP_AUDIO_STACK_MONO_REF
  // Fill ctx.spk_ref_buffer for the mono AEC path (ring buffer or previous
  // frame, zero-filled only when no reference exists). TDM and stereo paths
  // pre-fill the buffer during RX deinterleave and must not call this.
  void fill_mono_aec_reference_(AudioTaskCtx &ctx);
#endif

  // Pre-allocate audio task working buffers as soon as the processor frame
  // shape is known. Buffers persist across internal stop()+start() cycles so
  // the task can re-enter without calling heap_caps_alloc, which fragments
  // SPIRAM over time and causes spurious allocation failures on feature-toggle
  // sequences.
  bool prepare_audio_context_(AudioTaskCtx &ctx, bool require_processor_spec, bool log_context);
  bool allocate_audio_buffers_(AudioTaskCtx &ctx);
  void request_audio_preallocation_();
  void preallocate_audio_buffers_from_task_();

  // Pin configuration
  int lrclk_pin_{-1};
  int bclk_pin_{-1};
  int mclk_pin_{-1};
  int din_pin_{-1};   // Mic data in
  int dout_pin_{-1};  // Speaker data out

  uint32_t sample_rate_{16000};
  uint8_t bits_per_sample_{16};        // I2S bus bit depth: 16 or 32
  bool correct_dc_offset_{false};      // IIR high-pass filter to remove mic DC bias
  uint8_t num_channels_{1};            // Physical STD TX bus channels: 1 or 2
  uint8_t speaker_channels_{1};        // Public ESPHome speaker stream channels
  bool i2s_mode_secondary_{false};     // false = primary controller, true = secondary peripheral
  bool use_apll_{false};               // Use APLL clock source (ESP32 original only)
  uint8_t i2s_num_{0};                 // I2S port number (0 or 1)
  uint32_t mclk_multiple_{256};        // MCLK multiple: 128, 256, 384, or 512
  uint8_t i2s_comm_fmt_{0};            // 0=philips, 1=msb, 2=pcm_short, 3=pcm_long
  bool mic_channel_right_{false};      // RX mono slot: false=LEFT, true=RIGHT
  bool rx_slot_mode_stereo_{false};    // STD RX reads both slots; mic_channel selects one in software
  bool tx_slot_right_{false};          // TX mono slot: false=LEFT (default), true=RIGHT
  uint8_t slot_bit_width_{0};          // 0 = auto (match bits_per_sample), or 16/24/32
  uint32_t output_sample_rate_{0};     // 0 = use sample_rate_ (no rate conversion)
  uint32_t rate_conversion_ratio_{1};  // sample_rate_ / output_sample_rate_ (computed in setup)
  uint8_t rate_cvt_complexity_{3};     // esp_ae_rate_cvt complexity, 1..3
  uint8_t rate_cvt_perf_type_{1};      // esp_ae_rate_cvt perf type: 0=memory, 1=speed

  // Espressif rate converters for mic and software-reference paths.
#ifdef USE_ESP_AUDIO_STACK_MULTI_RX
  MultiChannelAudioEffectsRateConverter rx_rate_converter_;  // Multi-channel: TDM/stereo RX path
#endif
#ifdef USE_ESP_AUDIO_STACK_MONO_RX
  AudioEffectsRateConverter mic_rate_converter_;  // Mono RX without TDM/stereo
#endif
#ifdef USE_ESP_AUDIO_STACK_MONO_REF
  AudioEffectsRateConverter play_ref_rate_converter_;  // Mono mode: bus-rate ref from play() converted in audio_task
#endif

  // I2S handles managed as a coordinated RX/TX pair
  i2s_chan_handle_t tx_handle_{nullptr};
  i2s_chan_handle_t rx_handle_{nullptr};
#ifdef USE_ESP_AUDIO_STACK_HARDWARE_CODEC
  CodecDevBackend codec_backend_;
#endif

#ifdef USE_ESP_AUDIO_STACK_DUAL_BUS
  struct I2SBusConfig {
    bool configured{false};
    uint8_t i2s_num{0};
    int lrclk_pin{-1};
    int bclk_pin{-1};
    int mclk_pin{-1};
    int din_pin{-1};
    int dout_pin{-1};
  };
  bool dual_i2s_bus_{false};
  I2SBusConfig rx_bus_;
  I2SBusConfig tx_bus_;
#endif

  // State
  std::atomic<bool> audio_stack_running_{false};
  // Cached presence flag read by audio_task_ on the hot path; kept in sync
  // with mic_consumers_ (below) under mic_consumers_mutex_ on register/unregister.
  std::atomic<bool> has_mic_consumers_{false};
  // Opaque consumer tokens; registration survives stop()+start() by construction.
  // Fixed-size array (cap = MAX_LISTENERS = 16) avoids std::vector + std::mutex
  // in this public header, keeping the include footprint minimal.
  void *mic_consumers_[MAX_LISTENERS]{};
  size_t mic_consumer_count_{0};
  SemaphoreHandle_t mic_consumers_mutex_{nullptr};
  std::atomic<bool> speaker_running_{false};
  std::atomic<bool> speaker_paused_{false};
  std::atomic<uint8_t> runtime_state_{static_cast<uint8_t>(AudioStackRuntimeState::IDLE)};
  std::atomic<uint8_t> i2s_hardware_state_{static_cast<uint8_t>(I2SHardwareState::UNPREPARED)};
  Trigger<> start_trigger_;
  Trigger<> idle_trigger_;
  Trigger<std::string> state_trigger_;
  Trigger<> mic_start_trigger_;
  Trigger<> mic_idle_trigger_;
  Trigger<> speaker_start_trigger_;
  Trigger<> speaker_idle_trigger_;
  // audio_task_idle_: true while the task is parked in its outer wait loop
  //                   (not owning I2S). Waiters use audio_state_waiter_ instead
  //                   of polling when maintenance needs a specific task state.
  std::atomic<bool> audio_task_idle_{true};
  std::atomic<TaskHandle_t> audio_state_waiter_{nullptr};
  TaskHandle_t audio_task_handle_{nullptr};

  // Cross-thread buffer operation request (main thread -> audio task, avoids concurrent ring buffer access)
  std::atomic<bool> request_speaker_reset_{false};
  std::atomic<bool> prealloc_requested_{false};
  std::atomic<bool> prealloc_attempted_{false};

  // Deferred I2S release: stop() sets this and returns; loop() deletes the
  // esp_driver_i2s channels after audio_task_idle_ flips.
  // Avoids a blocking poll on the main loop during stop_speaker / unregister_mic_consumer.
  std::atomic<bool> teardown_pending_{false};

  AudioStackRuntimeState compute_runtime_state_() const;
  static const char *runtime_state_to_string(AudioStackRuntimeState state);
  void update_runtime_state_();
  static const char *i2s_hardware_state_to_string(I2SHardwareState state);
  void set_i2s_hardware_state_(I2SHardwareState state);
  void log_memory_snapshot_(const char *label) const;
  void service_speaker_reset_();

#ifdef USE_ESP_AUDIO_STACK_TDM_REF_DIAGNOSTIC
  // TDM AEC reference health: diagnostic-only counter for slot-map bring-up.
  std::atomic<uint32_t> tdm_ref_silent_frames_{0};
#endif

  DcBlockerState dc_primary_;
  DcBlockerState dc_secondary_;

  // Mic data callbacks
  MicCallbackSlot mic_callbacks_[MAX_LISTENERS]{};  // Post-processor stream for MWW, VA, call components
  size_t mic_callback_count_{0};

  // Speaker output callbacks (for mixer pending_playback_frames tracking)
  SpeakerOutputCallbackSlot speaker_output_callbacks_[MAX_LISTENERS]{};
  size_t speaker_output_callback_count_{0};

  QueueHandle_t tx_completion_event_queue_{nullptr};
  QueueHandle_t tx_completion_record_queue_{nullptr};
  uint8_t *tx_completion_silence_{nullptr};
  size_t tx_completion_silence_bytes_{0};
  size_t tx_completion_queue_size_{0};
  size_t tx_completion_dma_buffer_bytes_{0};
  uint32_t tx_completion_dma_frames_{0};
  uint32_t tx_completion_bytes_per_frame_{0};
  volatile bool tx_completion_desync_{false};
  std::atomic<uint32_t> tx_completion_pending_real_records_{0};

  // Speaker ring buffer: stores data at bus rate (sample_rate_)
  RingBufferPtr speaker_buffer_;
  size_t speaker_buffer_size_{0};  // Actual allocated size (scales with rate_conversion_ratio_)
  uint32_t speaker_buffer_duration_ms_{500};

  // Optional AEC/AFE processor support.
  AudioProcessor *processor_{nullptr};
  std::atomic<bool> processor_enabled_{false};  // Runtime toggle (only enabled when processor_ is set)
  std::atomic<bool> processor_background_consumer_registered_{false};
  void sync_processor_background_consumer_();
#ifdef USE_ESP_AUDIO_STACK_MONO_REF
  int16_t *direct_aec_ref_{nullptr};  // TX-side converted reference storage/scratch (processor rate)
  bool direct_aec_ref_valid_{false};  // True after first previous_frame reference has been saved
#endif

#ifdef USE_ESP_AUDIO_STACK_RING_REF
  // AEC ring buffer reference (TYPE2-style, for no-codec setups)
  uint32_t aec_ref_buffer_ms_{80};     // Config: ring buffer size in ms
  RingBufferPtr aec_ref_ring_buffer_;  // Ring buffer for AEC ref (processor rate, post-volume)
#endif

  // Gain controls (atomic: written from main loop, read from audio task via snapshot)
  std::atomic<float> mic_gain_{1.0f};             // Public/debug value, applied AFTER AEC.
  std::atomic<int32_t> mic_gain_q31_{INT32_MAX};  // Official esp-audio-libs attenuation path.
  std::atomic<int8_t> mic_gain_boost_db_{0};      // >0 dB via official esp_ae_alc.
  std::atomic<float> input_gain_{1.0f};           // Public/debug value before the processor.
  std::atomic<int32_t> input_gain_q31_{INT32_MAX};
  std::atomic<float> input_gain_boost_{1.0f};
  std::atomic<float> master_volume_public_{1.0f};          // public/debug master value
  std::atomic<int32_t> hot_output_volume_q31_{INT32_MAX};  // combined hot-path fixed-point output volume
  std::atomic<int32_t> output_volume_q31_{INT32_MAX};      // media_player/speaker abstraction volume
  std::atomic<int32_t> master_volume_q31_{INT32_MAX};      // board master volume
  std::atomic<float> master_volume_linear_{1.0f};          // unclipped user value for hardware codec volume APIs
  float master_volume_min_db_{-49.0f};                     // 0..100% master curve floor; 0% remains hard mute
  void *mic_alc_handle_{nullptr};
  int8_t mic_alc_gain_db_{0};
  bool use_stereo_aec_ref_{false};  // ES8311 digital feedback: RX stereo with L=mic, R=ref
  bool ref_channel_right_{false};   // Which channel is AEC reference: false=L, true=R

  // TDM bus/reference (ES7210 in TDM mode)
  bool use_tdm_bus_{false};
  bool use_tdm_ref_{false};
  uint8_t tdm_total_slots_{4};
  uint8_t tdm_mic_slot_{0};         // TDM slot index for voice mic
  int8_t tdm_second_mic_slot_{-1};  // Optional second mic slot for dual-mic AFE
  uint8_t tdm_ref_slot_{1};         // TDM slot index for AEC reference
  uint8_t tdm_tx_slot_{0};          // TDM slot index for speaker TX
  bool tdm_slot_level_sensor_enabled_[8] = {false};
  std::atomic<bool> any_tdm_slot_level_sensor_enabled_{false};
  std::atomic<float> tdm_slot_level_dbfs_[8] = {};
  uint8_t tdm_slot_level_divider_{0};

  // AEC gating: only run echo canceller while speaker has recent real audio.
  std::atomic<uint32_t> last_speaker_audio_ms_{0};
  static constexpr uint32_t AEC_ACTIVE_TIMEOUT_MS{250};

  // Task configuration (defaults match ESP-IDF audio best practices)
  uint8_t task_priority_{19};  // Above lwIP(18), below WiFi(23)
  int8_t task_core_{0};        // Core 0: canonical Espressif AEC pattern; -1 = unpinned
  uint32_t task_stack_size_{8192};
  uint32_t dma_desc_num_{6};
  uint32_t dma_frame_num_{0};
  bool dma_frame_num_configured_{false};
  bool buffers_in_psram_{false};           // Non-DMA buffers in PSRAM (saves ~15KB internal RAM)
  bool audio_task_stack_in_psram_{false};  // Audio task stack in PSRAM (saves ~8KB internal RAM)
#ifdef USE_ESP_AUDIO_STACK_RING_REF
  bool aec_ref_ring_in_psram_{
      false};  // AEC reference ring in PSRAM (saves ~3-5 KB internal, costs ~13.6 us/frame Core 0)
#endif
  StackType_t *audio_task_stack_{nullptr};  // Owned when audio_task_stack_in_psram_ is true
  StaticTask_t audio_task_tcb_{};           // Static TCB for the permanent audio task
  uint16_t telemetry_log_interval_frames_{128};

  // Error propagation: set by audio_task_ on persistent I2S failures
  std::atomic<bool> has_i2s_error_{false};

  // Pre-allocated audio task buffers (owned by component, not by ctx).
  // Allocated on audio_task_ entry and reused while the current frame shape
  // fits. Runtime processor reconfigures can grow frame sizes, so
  // allocate_audio_buffers_ validates the shape and reallocates before the
  // task touches RX/rate-converter buffers.
  void release_audio_buffers_();
  int16_t *prealloc_rx_buffer_{nullptr};
  int16_t *prealloc_mic_buffer_{nullptr};
  int16_t *prealloc_processor_mic_buffer_{nullptr};
  int16_t *prealloc_spk_buffer_{nullptr};
  int16_t *prealloc_spk_ref_buffer_{nullptr};
  int16_t *prealloc_tx_ref_mono_buffer_{nullptr};
  int16_t *prealloc_aec_output_{nullptr};
#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
  int16_t *prealloc_tdm_tx_buffer_{nullptr};
  int16_t *prealloc_tx_silence_buffer_{nullptr};
#endif
#ifdef USE_ESP_AUDIO_STACK_STEREO_TX
  int16_t *prealloc_tx_interleave_buffer_{nullptr};
#endif
  int16_t *prealloc_tx_clock_buffer_{nullptr};
#ifdef USE_ESP_AUDIO_STACK_32BIT
  int16_t *prealloc_tx_32_buffer_{nullptr};
#endif
  size_t prealloc_rx_buffer_bytes_{0};
  size_t prealloc_mic_buffer_bytes_{0};
  size_t prealloc_processor_mic_buffer_bytes_{0};
  size_t prealloc_spk_buffer_bytes_{0};
  size_t prealloc_spk_ref_buffer_bytes_{0};
  size_t prealloc_tx_ref_mono_buffer_bytes_{0};
  size_t prealloc_aec_output_bytes_{0};
#ifdef USE_ESP_AUDIO_STACK_TDM_BUS
  size_t prealloc_tdm_tx_buffer_bytes_{0};
  size_t prealloc_tx_silence_buffer_bytes_{0};
#endif
#ifdef USE_ESP_AUDIO_STACK_STEREO_TX
  size_t prealloc_tx_interleave_buffer_bytes_{0};
#endif
  size_t prealloc_tx_clock_buffer_bytes_{0};
#ifdef USE_ESP_AUDIO_STACK_32BIT
  size_t prealloc_tx_32_buffer_bytes_{0};
#endif
  bool audio_buffers_allocated_{false};
#if defined(USE_ESP_AUDIO_STACK_STEREO_TX) && defined(USE_ESP_AUDIO_STACK_MONO_REF)
  void *tx_ref_ch_cvt_handle_{nullptr};
#endif
#ifdef USE_ESP_AUDIO_STACK_32BIT
  void *tx_bit_cvt_handle_{nullptr};
  uint8_t tx_bit_cvt_channels_{0};
#endif
};

// Native actions for YAML automations
template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<ESPAudioStack> {
 public:
  void play(const Ts &...x) override { this->parent_->start(); }
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<ESPAudioStack> {
 public:
  void play(const Ts &...x) override { this->parent_->stop(); }
};

template<typename... Ts> class StopAndWaitAction : public Action<Ts...>, public Parented<ESPAudioStack> {
 public:
  void play(const Ts &...x) override { this->parent_->stop_and_wait(); }
};

template<typename... Ts> class IsIdleCondition : public Condition<Ts...>, public Parented<ESPAudioStack> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_idle(); }
};

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32
