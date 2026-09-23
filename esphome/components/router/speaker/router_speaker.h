#pragma once

#ifdef USE_ESP32

#include "esphome/components/speaker/speaker.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <freertos/FreeRTOS.h>

#include <atomic>

namespace esphome::router {

class Router final : public Component, public speaker::Speaker {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }

  void setup() override;
  void loop() override;
  void dump_config() override;

  size_t play(const uint8_t *data, size_t length) override { return this->play(data, length, 0); }
  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;

  void start() override;
  void stop() override;
  void finish() override;

  bool has_buffered_data() const override;

  void set_pause_state(bool pause_state) override;
  bool get_pause_state() const override { return this->cached_pause_; }

  void set_volume(float volume) override;
  float get_volume() override { return this->volume_; }

  void set_mute_state(bool mute_state) override;
  bool get_mute_state() override { return this->mute_state_; }

  // Allocate the output list to its final size. Must be called before add_output().
  void set_output_count(size_t count) { this->outputs_.init(count); }
  void add_output(speaker::Speaker *spk) { this->outputs_.push_back(spk); }

  /// Switch the active output to the given speaker. Must be one of the configured outputs.
  /// Returns false if `target` is not in the output list.
  bool switch_to_output(speaker::Speaker *target);

  // Always valid: active_output_idx_ stays within [0, outputs_.size()) and at least
  // two outputs are required (validated in Python), so this never returns null.
  speaker::Speaker *get_active_output() const {
    return this->outputs_[this->active_output_idx_.load(std::memory_order_relaxed)];
  }

 protected:
  // Frames written to the active output but not yet played: incremented in play() and decremented
  // (clamped at zero) by the active output's audio_output_callback. Mirrors mixer_speaker's
  // frames_in_pipeline_.
  std::atomic<uint32_t> frames_in_pipeline_{0};

  bool cached_pause_{false};

  void apply_cached_state_to_active_();

  // Index of the previously-active output we're waiting on to fully stop before
  // starting the new one. -1 means no pending start. Set by switch_to_output()
  // when switching mid-playback; cleared by loop() once the old output reports
  // is_stopped(). Required because shared-bus drivers (e.g. two i2s_audio
  // speakers on one i2s_bus) reject start() until the previous user releases.
  std::atomic<int8_t> pending_start_prev_idx_{-1};

 private:
  FixedVector<speaker::Speaker *> outputs_;
  // Index into outputs_, always within [0, outputs_.size()). Defaults to the first
  // configured output; updated by switch_to_output().
  std::atomic<int8_t> active_output_idx_{0};
};

template<typename... Ts> class SwitchOutputAction final : public Action<Ts...> {
 public:
  explicit SwitchOutputAction(Router *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(speaker::Speaker *, target)
  void play(const Ts &...x) override { this->parent_->switch_to_output(this->target_.value(x...)); }

 protected:
  Router *parent_;
};

}  // namespace esphome::router

#endif  // USE_ESP32
