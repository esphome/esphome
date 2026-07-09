#pragma once

#include "../esp_audio_stack/audio_core_processor.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#ifdef USE_ESP32

#include <esp_afe_aec.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace esphome {
namespace esp_aec {

using esp_audio_stack::AudioFeature;
using esp_audio_stack::AudioProcessor;
using esp_audio_stack::FeatureControl;
using esp_audio_stack::FrameSpec;
using esp_audio_stack::ProcessorTelemetry;

/// Standalone Espressif AEC wrapper.
///
/// Implements AudioProcessor with only the echo canceller enabled
/// (WebRTC or neural, depending on mode). Lower memory footprint than
/// EspAfe. Intended for devices that only need echo cancellation without the
/// full AFE pipeline.
///
/// Consumers see AEC as just another
/// AudioProcessor; picking EspAec vs EspAfe is a YAML choice.
class EspAec : public Component, public AudioProcessor {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  // Config setters (called from Python)
  void set_sample_rate(int sample_rate) { this->sample_rate_ = sample_rate; }
  void set_filter_length(int filter_length) { this->filter_length_ = filter_length; }
  void set_mode(int mode) { this->mode_ = static_cast<aec_mode_t>(mode); }

  // AudioProcessor interface
  bool is_initialized() const override { return this->handle_ != nullptr; }
  FrameSpec frame_spec() const override;
  bool process(const int16_t *in_mic, const int16_t *in_ref, int16_t *out, uint8_t mic_channels_in) override;
  FeatureControl feature_control(AudioFeature feature) const override;
  bool set_feature(AudioFeature feature, bool enabled) override;
  ProcessorTelemetry telemetry() const override;
  bool reconfigure(int type, int mode) override;
  uint32_t frame_spec_revision() const override { return this->frame_spec_revision_.load(std::memory_order_acquire); }

  aec_mode_t get_mode() const { return this->mode_; }
  static const char *get_mode_name(aec_mode_t mode);
  const char *get_mode_name() const { return get_mode_name(this->mode_); }

  ~EspAec() override;

 protected:
  bool reinit_(aec_mode_t new_mode);
  static afe_type_t afe_type_from_mode_(aec_mode_t mode);
  static afe_mode_t afe_cost_from_mode_(aec_mode_t mode);

  afe_aec_handle_t *handle_{nullptr};
  int16_t *input_frame_{nullptr};
  // L1 fix: guards handle_ lifecycle between process() and reinit_() / destructor.
  SemaphoreHandle_t handle_mutex_{nullptr};
  int sample_rate_{16000};
  int filter_length_{4};
  int cached_frame_size_{512};
  aec_mode_t mode_{AEC_MODE_SR_LOW_COST};
  std::atomic<uint32_t> frame_spec_revision_{0};
  int last_frame_size_{0};
};

// Action: esp_aec.set_mode
template<typename... Ts> class SetModeAction : public Action<Ts...>, public Parented<EspAec> {
 public:
  TEMPLATABLE_VALUE(std::string, mode)
  void play(const Ts &...x) override {
    std::string name = this->mode_.value(x...);
    // Decode the user-facing mode string into the (type, cost) pair that
    // EspAec::reconfigure expects. type: 0=SR, 1=VC, 2=FD; cost: 0=low, 1=high.
    int type;
    int cost;
    if (name == "sr_low_cost") {
      type = 0;
      cost = 0;
    } else if (name == "sr_high_perf") {
      type = 0;
      cost = 1;
    } else if (name == "voip_low_cost") {
      type = 1;
      cost = 0;
    } else if (name == "voip_high_perf") {
      type = 1;
      cost = 1;
    } else if (name == "fd_low_cost") {
      type = 2;
      cost = 0;
    }  // esp-sr 2.4+
    else if (name == "fd_high_perf") {
      type = 2;
      cost = 1;
    }  // esp-sr 2.4+
    else
      return;
    this->parent_->reconfigure(type, cost);
  }
};

}  // namespace esp_aec
}  // namespace esphome

#endif  // USE_ESP32
