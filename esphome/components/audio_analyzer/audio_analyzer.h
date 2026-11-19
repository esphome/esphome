#pragma once

#include "analysis.h"

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::audio_analyzer {

/// @brief Live power and spectrum of an audio stream, for visualizers, level meters and light effects.
///
/// Audio arrives on another task and is only copied there. The analysis (``SpectrumAnalysis``) runs in ``loop()`` on
/// the main task, so the getters can be called from lambdas (light effects, templates) without locking. Streams are
/// downmixed to mono and decimated to at most 16 kHz, then analyzed in frames of 512 samples (32 ms at 16 kHz).
///
/// The getters always return a number: without audio the values release towards silence (-100 dBFS, level 0) and
/// ``is_active()`` turns false.
class AudioAnalyzer : public Component {
 public:
  /// Smoothed power in dBFS, -100 when silent.
  float get_power_db() const { return this->analysis_.get_power_db(); }
  /// Smoothed power mapped linearly from the power range to 0..1.
  float get_power_level() const { return this->analysis_.get_power_level(); }
  /// Smoothed power of a band in dBFS, -100 when silent. Band 0 is the lowest frequency.
  float get_band_db(uint8_t band) const { return this->analysis_.get_band_db(band); }
  /// Smoothed power of a band mapped linearly from the band range to 0..1.
  float get_band_level(uint8_t band) const { return this->analysis_.get_band_level(band); }
  static constexpr uint8_t get_num_bands() { return NUM_BANDS; }
  /// True while audio is being analyzed.
  bool is_active() const { return this->active_; }

  /// @brief Registers a callback called on the main task after each analyzed frame.
  template<typename F> void add_on_frame_callback(F &&callback) {
    this->frame_callback_.add(std::forward<F>(callback));
  }

  void set_frequency_range(float min_hz, float max_hz) { this->analysis_.set_frequency_range(min_hz, max_hz); }
  void set_power_range(float min_db, float max_db) { this->analysis_.set_power_range(min_db, max_db); }
  void set_band_range(float min_db, float max_db) { this->analysis_.set_band_range(min_db, max_db); }
  void set_attack(uint32_t attack_ms) { this->analysis_.set_attack(attack_ms); }
  void set_release(uint32_t release_ms) { this->analysis_.set_release(release_ms); }

 protected:
  void dump_analysis_config_();

  /// @brief Sets the band plan for an analysis rate. Warns when the maximum frequency is above what the rate allows.
  void configure_analysis_(float analysis_rate);

  /// @brief Analyzes one sample at the analysis rate.
  void push_sample_(float sample) {
    if (this->analysis_.push(sample)) {
      this->on_frame_();
    }
  }

  /// @brief Releases the outputs towards silence while no frames arrive. Call it from ``loop()``.
  void release_if_idle_();

  void on_frame_();

  SpectrumAnalysis analysis_;
  LazyCallbackManager<void()> frame_callback_;
  uint32_t last_frame_ms_{0};
  bool active_{false};
};

}  // namespace esphome::audio_analyzer
