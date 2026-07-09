#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#include "../esp_audio_stack/audio_core_processor.h"
#include "../esp_audio_stack/audio_core_task_utils.h"

#ifdef USE_ESP32

#include <esp_afe_sr_iface.h>
#include <esp_afe_sr_models.h>
#include <esp_afe_config.h>
#ifdef USE_ESP_AFE_GMF_PATH
#include <esp_gmf_afe.h>
#include <esp_gmf_element.h>
#include <esp_gmf_afe_manager.h>
#include <esp_gmf_pipeline.h>
#include <esp_gmf_payload.h>
#include <esp_gmf_port.h>
#include <esp_gmf_task.h>
#endif
#include <freertos/queue.h>
#include <freertos/ringbuf.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "../esp_audio_stack/audio_core_ring_buffer_caps.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace esphome {
namespace esp_afe {

using esp_audio_stack::AudioFeature;
using esp_audio_stack::AudioProcessor;
using esp_audio_stack::FeatureControl;
using esp_audio_stack::FrameSpec;
using esp_audio_stack::ProcessorTelemetry;

/// Full Espressif AFE pipeline wrapper.
///
/// Implements AudioProcessor on top of Espressif's `esp_gmf_afe` element in a
/// GMF pipeline/task (AEC + NS + VAD + AGC + structural dual-mic Speech
/// Enhancement). This component keeps the ESPHome-facing `process()` contract
/// as a bounded enqueue/dequeue bridge; the AFE work itself runs through GMF.
///
/// Runtime reconfiguration that changes the AFE graph (NS/AGC or switching
/// SR/VC/FD mode) must tear the esp-sr instance down and rebuild it. AEC is
/// rebuild-only on the ESP-SR single-mic direct path and live-toggled only
/// through the GMF manager; VAD and SE/BSS are structural on current builds.
/// Because
/// process() is called from the consumer audio task (prio 19) while
/// config mutations come from the main thread, the two ends coordinate
/// through a lock-free drain handshake:
///
///   1. Config change acquires config_mutex_ and flips drain_request_
///      to true.
///   2. process() observes drain_request_ at the top of every frame and
///      returns silence immediately, without touching the esp-sr handle.
///   3. Config change waits for process_busy_ to clear, then it is safe
///      to destroy + rebuild the esp-sr instance.
///   4. After rebuild, drain_request_ is cleared; process() resumes
///      normal operation on the next frame.
///
/// This avoids blocking the consumer's audio task on any global mutex.
class EspAfe : public Component, public AudioProcessor {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  // AudioProcessor interface
  bool is_initialized() const override {
#ifdef USE_ESP_AFE_DIRECT_PATH
    if (this->direct_data_ != nullptr && this->direct_feed_signal_ != nullptr) {
      return this->feed_buf_ != nullptr;
    }
#endif
#ifdef USE_ESP_AFE_GMF_PATH
    if (this->afe_manager_ != nullptr && this->feed_input_ring_ != nullptr) {
      return true;
    }
#endif
    return false;
  }
  FrameSpec frame_spec() const override;
  bool process(const int16_t *in_mic, const int16_t *in_ref, int16_t *out, uint8_t mic_channels_in) override;
  uint32_t frame_spec_revision() const override { return this->frame_spec_revision_.load(std::memory_order_acquire); }
  FeatureControl feature_control(AudioFeature feature) const override;
  bool set_feature(AudioFeature feature, bool enabled) override;
  ProcessorTelemetry telemetry() const override;
  bool reconfigure(int type, int mode) override;
  // Stop the GMF AFE pipeline when no consumer is listening (called by
  // esp_audio_stack when the last mic consumer leaves) and run it when a
  // consumer re-attaches. Idempotent.
  void set_processing_active(bool active) override;
  bool wants_background_input() const override {
    return this->continuous_vad_ && this->vad_enabled_.load(std::memory_order_relaxed);
  }

  // Config setters (called from Python codegen)
  void set_afe_type(int type) { this->afe_type_ = type; }
  void set_afe_mode(int mode) { this->afe_mode_ = mode; }
  void set_mic_num(int num) { this->mic_num_ = num; }
  void set_aec_enabled(bool en) { this->aec_enabled_.store(en, std::memory_order_relaxed); }
  void set_aec_filter_length(int len) { this->aec_filter_length_ = len; }
  void set_aec_nlp_level(int level) { this->aec_nlp_level_ = level; }
  void set_se_enabled(bool en) { this->se_enabled_.store(en, std::memory_order_relaxed); }
  void set_ns_enabled(bool en) { this->ns_enabled_.store(en, std::memory_order_relaxed); }
  void set_vad_enabled(bool en) { this->vad_enabled_.store(en, std::memory_order_relaxed); }
  void set_vad_mode(int mode) { this->vad_mode_ = mode; }
  void set_vad_min_speech_ms(int ms) { this->vad_min_speech_ms_ = ms; }
  void set_vad_min_noise_ms(int ms) { this->vad_min_noise_ms_ = ms; }
  void set_vad_delay_ms(int ms) { this->vad_delay_ms_ = ms; }
  void set_vad_mute_playback(bool en) { this->vad_mute_playback_ = en; }
  void set_vad_enable_channel_trigger(bool en) { this->vad_enable_channel_trigger_ = en; }
  void set_continuous_vad(bool en) { this->continuous_vad_ = en; }
  void set_agc_enabled(bool en) { this->agc_enabled_.store(en, std::memory_order_relaxed); }
  void set_agc_compression_gain(int gain) { this->agc_compression_gain_ = gain; }
  void set_agc_target_level(int level) { this->agc_target_level_ = level; }
  void set_memory_alloc_mode(int mode) { this->memory_alloc_mode_ = mode; }
  void set_afe_linear_gain(float gain) { this->afe_linear_gain_ = gain; }
  void set_task_core(int core) { this->task_core_ = core; }
  void set_task_priority(int prio) { this->task_priority_ = prio; }
  void set_ringbuf_size(int size) { this->ringbuf_size_ = size; }
  void set_feed_task_core(int core) { this->feed_task_core_ = core; }
  void set_feed_task_priority(int prio) { this->feed_task_priority_ = prio; }
  void set_feed_task_stack_size(int size) { this->feed_task_stack_size_ = size; }
  void set_fetch_task_core(int core) { this->fetch_task_core_ = core; }
  void set_fetch_task_priority(int prio) { this->fetch_task_priority_ = prio; }
  void set_fetch_task_stack_size(int size) { this->fetch_task_stack_size_ = size; }
  void set_input_format_override(const char *fmt);
  void set_feed_buf_in_psram(bool psram) { this->feed_buf_in_psram_ = psram; }
  void set_feed_ring_in_psram(bool psram) { this->feed_ring_in_psram_ = psram; }
  void set_fetch_ring_in_psram(bool psram) { this->fetch_ring_in_psram_ = psram; }
  void set_input_volume_sensor_enabled(bool en) { this->input_volume_sensor_enabled_ = en; }
  void set_output_rms_sensor_enabled(bool en) { this->output_rms_sensor_enabled_ = en; }

  // Runtime toggles (for switches and automations)
  bool enable_aec();
  bool disable_aec();
  bool enable_ns();
  bool disable_ns();
  bool enable_vad();
  bool disable_vad();
  bool enable_agc();
  bool disable_agc();

  bool is_aec_enabled() const { return this->aec_enabled_.load(std::memory_order_relaxed); }
  bool is_se_enabled() const { return this->mic_num_ >= 2; }
  bool is_ns_enabled() const { return this->ns_enabled_.load(std::memory_order_relaxed); }
  bool is_vad_enabled() const { return this->vad_enabled_.load(std::memory_order_relaxed); }
  bool is_agc_enabled() const { return this->agc_enabled_.load(std::memory_order_relaxed); }
  // Current mode string ("sr_low_cost", "sr_high_perf", "voip_low_cost",
  // "voip_high_perf", "fd_low_cost", "fd_high_perf"). Used by UI templates
  // to publish the actual live mode after a set_action so optimistic selects
  // don't drift from reality.
  std::string get_mode_name() const {
    // afe_type_: 0 = SR, 1 = VC, 3 = FD (esp-sr 2.4+); afe_mode_: 0 = LOW_COST, 1 = HIGH_PERF.
    const bool high = (this->afe_mode_ == 1);
    switch (this->afe_type_) {
      case 0:
        return high ? "sr_high_perf" : "sr_low_cost";
      case 3:
        return high ? "fd_high_perf" : "fd_low_cost";
      default:
        return high ? "voip_high_perf" : "voip_low_cost";
    }
  }
  bool is_voice_present() const { return this->voice_present_.load(std::memory_order_relaxed); }
  float get_input_volume_dbfs() const { return this->input_volume_dbfs_.load(std::memory_order_relaxed); }
  float get_output_rms_dbfs() const { return this->output_rms_dbfs_.load(std::memory_order_relaxed); }

  // Reinit with a new mode string (e.g. "sr_low_cost", "voip_high_perf").
  // Caller must stop audio processing before calling this.
  bool reinit_by_name(const std::string &name);
  bool reinit_by_name(const char *name);
  bool request_reinit_by_name(const std::string &name);
  bool request_reinit_by_name(const char *name);
  bool is_reconfigure_idle() const;
  bool get_last_reconfigure_ok() const { return this->last_reconfigure_ok_.load(std::memory_order_acquire); }

  ~EspAfe() override;

 protected:
  // Derive aec_mode_t from afe_type + afe_mode
  aec_mode_t derive_aec_mode_() const;
  int afe_mic_channels_() const;

  struct AfeInstance {
#ifdef USE_ESP_AFE_DIRECT_PATH
    const esp_afe_sr_iface_t *direct_iface{nullptr};
    esp_afe_sr_data_t *direct_data{nullptr};
#endif
#ifdef USE_ESP_AFE_GMF_PATH
    esp_gmf_afe_manager_handle_t manager{nullptr};
    esp_gmf_obj_handle_t element{nullptr};
    esp_gmf_pipeline_handle_t pipeline{nullptr};
    esp_gmf_task_handle_t task{nullptr};
#endif
    afe_config_t *config{nullptr};
    int16_t *feed_buf{nullptr};
    int feed_chunksize{0};
    int fetch_chunksize{0};
    int process_chunksize{0};
    int total_channels{0};
  };

  bool build_instance_(AfeInstance *instance);
  bool recreate_instance_(bool require_same_frame_sizes);
  void clear_process_busy_();
  bool start_reconfigure_task_();
  static void reconfigure_task_trampoline_(void *arg);
  void reconfigure_task_loop_();
  bool set_aec_enabled_runtime_(bool enabled);
  bool set_vad_enabled_runtime_(bool enabled);
  bool set_reinit_flag_(std::atomic<bool> &flag, bool enabled, const char *name);
  bool prepare_runtime_();
  bool prepare_fetch_output_ring_();
#ifdef USE_ESP_AFE_GMF_PATH
  bool prepare_feed_input_ring_();
#endif
  void release_runtime_buffers_();
  void log_memory_snapshot_(const char *label) const;
  void destroy_instance_(AfeInstance *instance);
  bool install_instance_(AfeInstance *instance);
  AfeInstance detach_instance_();
  const char *memory_alloc_mode_to_str_() const;

  // True when the user has every AFE feature turned off. In that state
  // running the pipeline is pointless, so recreate_instance_ and the runtime
  // toggle paths tear the instance down instead of rebuilding it.
  bool all_features_disabled_() const {
    return !this->aec_enabled_.load(std::memory_order_relaxed) && !this->ns_enabled_.load(std::memory_order_relaxed) &&
           !this->agc_enabled_.load(std::memory_order_relaxed) && !this->vad_enabled_.load(std::memory_order_relaxed) &&
           !this->is_se_enabled();
  }

  // GMF AFE manager and config. The config must outlive the manager because
  // esp-sr stores pointers into it.
#ifdef USE_ESP_AFE_DIRECT_PATH
  const esp_afe_sr_iface_t *direct_iface_{nullptr};
  esp_afe_sr_data_t *direct_data_{nullptr};
#endif
#ifdef USE_ESP_AFE_GMF_PATH
  esp_gmf_afe_manager_handle_t afe_manager_{nullptr};
  esp_gmf_obj_handle_t afe_element_{nullptr};
  esp_gmf_pipeline_handle_t afe_pipeline_{nullptr};
  esp_gmf_task_handle_t afe_task_{nullptr};
#endif
  afe_config_t *afe_config_{nullptr};
  bool afe_pipeline_running_{false};
  bool afe_pipeline_paused_{false};
#ifdef USE_ESP_AFE_DIRECT_PATH
  bool direct_fetch_running_{false};
#endif

  // Feed buffer: interleaved [mic, ref, ...], [mic1, mic2, ref, ...] or
  // [mic1, mic2, N, ref, ...] depending on esp-sr input_format.
  int16_t *feed_buf_{nullptr};
  int feed_chunksize_{0};     // per-channel samples expected by feed()
  int fetch_chunksize_{0};    // mono output samples returned by fetch()
  int process_chunksize_{0};  // external process() input chunk size
  int total_channels_{2};
  int staged_input_samples_{0};
  // Last mic_channels_in seen by process(); used to drop a partial
  // staged frame if the consumer flips the channel layout without
  // passing through recreate_instance_ first.
  int last_process_mic_channels_{0};

  // esp_audio_stack stages full AFE feed frames into this NOSPLIT bridge.
  // The official esp_gmf_afe element reads them through a GMF input port and
  // writes processed mono frames through a GMF output port.
  static constexpr size_t kBridgeRingFrames = 4;
  static constexpr size_t kRingbufferItemHeaderBytes = 8;

#ifdef USE_ESP_AFE_GMF_PATH
  RingbufHandle_t feed_input_ring_{nullptr};
  uint8_t *feed_input_ring_storage_{nullptr};
  StaticRingbuffer_t *feed_input_ring_struct_{nullptr};
#endif
#ifdef USE_ESP_AFE_DIRECT_PATH
  SemaphoreHandle_t direct_feed_signal_{nullptr};
  StaticSemaphore_t direct_feed_signal_storage_{};
  std::atomic<TaskHandle_t> direct_fetch_stop_waiter_{nullptr};
  static constexpr int kDirectFeedSignalMaxCount = 8;
  TaskHandle_t direct_fetch_task_handle_{nullptr};
  StaticTask_t direct_fetch_task_tcb_{};
  StackType_t *direct_fetch_task_stack_{nullptr};
  static constexpr uint32_t kDirectFetchTaskStackWords = 4096 / sizeof(StackType_t);
#endif

#ifdef USE_ESP_AFE_GMF_PATH
  static esp_gmf_err_io_t gmf_input_acquire_cb_(void *ctx, esp_gmf_payload_t *load, uint32_t wanted_size,
                                                int wait_ticks);
  static esp_gmf_err_io_t gmf_input_release_cb_(void *ctx, esp_gmf_payload_t *load, int wait_ticks);
  static esp_gmf_err_io_t gmf_output_acquire_cb_(void *ctx, esp_gmf_payload_t *load, uint32_t wanted_size,
                                                 int wait_ticks);
  static esp_gmf_err_io_t gmf_output_release_cb_(void *ctx, esp_gmf_payload_t *load, int wait_ticks);
  esp_gmf_err_io_t gmf_input_acquire_(esp_gmf_payload_t *load, uint32_t wanted_size, int wait_ticks);
  esp_gmf_err_io_t gmf_output_release_(esp_gmf_payload_t *load, int wait_ticks);
#endif
  void handle_manager_result_(afe_fetch_result_t *result);
#ifdef USE_ESP_AFE_DIRECT_PATH
  static void direct_fetch_task_trampoline_(void *arg);
  void direct_fetch_task_loop_();
  bool start_direct_fetch_task_();
  void stop_direct_fetch_task_();
#endif
#ifdef USE_ESP_AFE_GMF_PATH
  static void gmf_event_cb_(esp_gmf_element_handle_t el, esp_gmf_afe_evt_t *event, void *user_data);
#endif
  bool start_pipeline_();
  bool pause_pipeline_();
  void stop_pipeline_();
#ifdef USE_ESP_AFE_GMF_PATH
  void flush_pipeline_before_stop_();
  void drain_feed_input_ring_();
  void *acquire_gmf_feed_slot_(size_t feed_bytes, TickType_t ticks_to_wait);
  bool commit_gmf_feed_slot_(void *slot);
#endif
  void update_fetch_ring_free_pct_();

  // Fetch bridge: GMF output port writes, process() reads non-blocking.
  esp_audio_stack::RingBufferPtr fetch_output_ring_;

  // Config (set from Python, used in setup())
  int afe_type_{0};  // AFE_TYPE_SR
  int afe_mode_{0};  // AFE_MODE_LOW_COST
  int mic_num_{1};   // physical microphone channels available from transport
  // Feature flags exposed to UI toggles. std::atomic so a torn snapshot
  // across the 5 flags in all_features_disabled_() can't trigger a
  // spurious teardown if a future caller reads them off the main loop.
  std::atomic<bool> aec_enabled_{true};
  int aec_filter_length_{4};
  int aec_nlp_level_{1};  // AEC_NLP_LEVEL_AGGR
  std::atomic<bool> se_enabled_{false};
  std::atomic<bool> ns_enabled_{true};
  std::atomic<bool> vad_enabled_{false};
  int vad_mode_{VAD_MODE_3};
  int vad_min_speech_ms_{128};
  int vad_min_noise_ms_{1000};
  int vad_delay_ms_{128};
  bool vad_mute_playback_{false};
  bool vad_enable_channel_trigger_{false};
  bool continuous_vad_{false};
  std::atomic<bool> agc_enabled_{true};
  int agc_compression_gain_{9};
  int agc_target_level_{3};
  int memory_alloc_mode_{AFE_MEMORY_ALLOC_MORE_PSRAM};
  float afe_linear_gain_{1.0f};
  int task_core_{1};
  int task_priority_{5};
  int ringbuf_size_{8};
  int feed_task_core_{0};
  int feed_task_priority_{5};
  int feed_task_stack_size_{3072};
  int fetch_task_core_{1};
  int fetch_task_priority_{5};
  int fetch_task_stack_size_{3072};
  char input_format_override_[5]{};
  bool feed_buf_in_psram_{false};    // Direct/split-frame scratch placement; 1:1 GMF feeds use ring slots.
  bool feed_ring_in_psram_{false};   // ~12 KB staging ring (default internal, ~20 us/frame faster on Core 0)
  bool fetch_ring_in_psram_{false};  // ~4 KB output ring (default internal, ~6.8 us/frame faster on Core 0)

  // config_mutex_ serialises config-change paths (recreate_instance_,
  // set_aec_enabled_runtime_, destructor). It is NOT taken by process() on
  // the hot path. process() uses the drain protocol below instead.
  SemaphoreHandle_t config_mutex_{nullptr};

  // Drain protocol for process() vs recreate_instance_. The busy/request
  // store-load pairs use seq_cst ordering; release/acquire on different
  // atomics would allow both cores to observe false and enter teardown/process
  // concurrently.
  std::atomic<bool> drain_request_{false};
  std::atomic<bool> process_busy_{false};
  std::atomic<TaskHandle_t> process_drain_waiter_{nullptr};

  struct ReconfigureRequest {
    char mode[32]{};
  };
  static constexpr uint32_t kReconfigureTaskStackBytes = 12288;
  QueueHandle_t reconfigure_queue_{nullptr};
  StaticQueue_t reconfigure_queue_struct_{};
  uint8_t reconfigure_queue_storage_[sizeof(ReconfigureRequest)]{};
  TaskHandle_t reconfigure_task_handle_{nullptr};
  StaticTask_t reconfigure_task_tcb_{};
  StackType_t *reconfigure_task_stack_{nullptr};
  std::atomic<bool> reconfigure_task_running_{false};
  std::atomic<bool> reconfigure_busy_{false};
  std::atomic<bool> last_reconfigure_ok_{true};

  // afe_stopped_ == true means the GMF pipeline/manager is torn down for a
  // single-mic all-features-off configuration. Dual-mic builds keep SE/BSS
  // structural, so this path should not be entered there.
  std::atomic<bool> afe_stopped_{false};

  std::atomic<bool> voice_present_{false};
  std::atomic<float> input_volume_dbfs_{-120.0f};
  std::atomic<float> output_rms_dbfs_{-120.0f};
  bool input_volume_sensor_enabled_{false};
  bool output_rms_sensor_enabled_{false};
  uint8_t rms_sensor_divider_{0};
  int warmup_remaining_{3};
  // Feed/fetch diagnostics.
  std::atomic<uint32_t> input_ring_drop_{0};   // process() could not enqueue (NOSPLIT full)
  std::atomic<uint32_t> feed_ok_{0};           // GMF input port accepted a frame
  std::atomic<uint32_t> feed_rejected_{0};     // GMF input port timed out or saw a bad frame
  std::atomic<uint32_t> fetch_ok_{0};          // GMF output port drained a frame
  std::atomic<uint32_t> fetch_timeout_{0};     // GMF fetch returned no usable frame
  std::atomic<uint32_t> output_ring_drop_{0};  // fetch_output_ring_ full
  std::atomic<uint32_t> feed_queue_frames_{0};
  std::atomic<uint32_t> feed_queue_peak_{0};
  std::atomic<uint32_t> fetch_queue_frames_{0};
  std::atomic<uint32_t> fetch_queue_peak_{0};
  std::atomic<uint32_t> process_us_last_{0};
  std::atomic<uint32_t> process_us_max_{0};
  std::atomic<uint32_t> feed_us_last_{0};
  std::atomic<uint32_t> feed_us_max_{0};
  std::atomic<uint32_t> fetch_us_last_{0};
  std::atomic<uint32_t> fetch_us_max_{0};
  std::atomic<float> ringbuf_free_pct_{1.0f};
  std::atomic<uint32_t> frame_spec_revision_{0};
  std::atomic<TaskHandle_t> pipeline_flush_waiter_{nullptr};
  // Tracks whether a microphone consumer wants the AFE path active. When the
  // pipeline is live, this also means GMF jobs are running; when an all-off
  // single-mic config has torn the pipeline down, the flag is preserved
  // so a later feature-enable rebuild can resume immediately.
  std::atomic<bool> processing_active_{false};
  int last_spec_mic_ch_{1};  // last published mic_channels for revision tracking (1 = default mono)
  int last_spec_process_size_{0};
  int last_spec_fetch_size_{0};
};

#ifdef USE_SWITCH
class AfeSwitchBase : public switch_::Switch, public Component, public Parented<EspAfe> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void setup() override {
    if (this->parent_ == nullptr)
      return;

    auto initial = this->get_initial_state_with_restore_mode();
    if (initial.has_value()) {
      if (this->parent_->is_initialized()) {
        this->write_state(*initial);
      } else {
        this->apply_initial_state_(*initial);
        this->publish_state(this->get_parent_state_());
      }
    } else {
      this->publish_state(this->get_parent_state_());
    }
  }

 protected:
  virtual bool get_parent_state_() const = 0;
  virtual void apply_initial_state_(bool state) {}

  void publish_parent_state_() {
    if (this->parent_ != nullptr) {
      this->publish_state(this->get_parent_state_());
    }
  }
};

// Switch platform classes
class AfeAecSwitch : public AfeSwitchBase {
 public:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;
    if ((state && this->parent_->enable_aec()) || (!state && this->parent_->disable_aec())) {
      this->publish_state(state);
    } else {
      this->publish_parent_state_();
    }
  }

 protected:
  bool get_parent_state_() const override { return this->parent_->is_aec_enabled(); }
  void apply_initial_state_(bool state) override { this->parent_->set_aec_enabled(state); }
};

class AfeNsSwitch : public AfeSwitchBase {
 public:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;
    if ((state && this->parent_->enable_ns()) || (!state && this->parent_->disable_ns())) {
      this->publish_state(state);
    } else {
      this->publish_parent_state_();
    }
  }

 protected:
  bool get_parent_state_() const override { return this->parent_->is_ns_enabled(); }
  void apply_initial_state_(bool state) override { this->parent_->set_ns_enabled(state); }
};

class AfeVadSwitch : public AfeSwitchBase {
 public:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;
    if ((state && this->parent_->enable_vad()) || (!state && this->parent_->disable_vad())) {
      this->publish_state(state);
    } else {
      this->publish_parent_state_();
    }
  }

 protected:
  bool get_parent_state_() const override { return this->parent_->is_vad_enabled(); }
  void apply_initial_state_(bool state) override { this->parent_->set_vad_enabled(state); }
};

class AfeAgcSwitch : public AfeSwitchBase {
 public:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;
    if ((state && this->parent_->enable_agc()) || (!state && this->parent_->disable_agc())) {
      this->publish_state(state);
    } else {
      this->publish_parent_state_();
    }
  }

 protected:
  bool get_parent_state_() const override { return this->parent_->is_agc_enabled(); }
  void apply_initial_state_(bool state) override { this->parent_->set_agc_enabled(state); }
};
#endif  // USE_SWITCH

#ifdef USE_BINARY_SENSOR
class AfeVadBinarySensor : public binary_sensor::BinarySensor, public PollingComponent, public Parented<EspAfe> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->is_voice_present());
    }
  }

  void update() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->is_voice_present());
    }
  }
};
#endif  // USE_BINARY_SENSOR

#ifdef USE_SENSOR
class AfeInputVolumeSensor : public sensor::Sensor, public PollingComponent, public Parented<EspAfe> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void update() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_input_volume_dbfs());
    }
  }
};

class AfeOutputRmsSensor : public sensor::Sensor, public PollingComponent, public Parented<EspAfe> {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void update() override {
    if (this->parent_ != nullptr) {
      this->publish_state(this->parent_->get_output_rms_dbfs());
    }
  }
};
#endif  // USE_SENSOR

// Action: esp_afe.set_mode
template<typename... Ts> class SetModeAction : public Action<Ts...>, public Parented<EspAfe> {
 public:
  TEMPLATABLE_VALUE(std::string, mode)
  void play(const Ts &...x) override { this->parent_->request_reinit_by_name(this->mode_.value(x...)); }
};

}  // namespace esp_afe
}  // namespace esphome

#endif  // USE_ESP32
