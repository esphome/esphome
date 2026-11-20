#pragma once

#include "analysis.h"
#include "stream_tap.h"

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <array>
#include <utility>

namespace esphome::audio_analyzer {

/// Live RMS level and spectrum of an audio stream. The audio task only copies the audio (``StreamTap``); the analysis
/// runs in ``loop()``, so lambdas can call the getters. Without audio the values release to silence.
class AudioAnalyzer : public Component {
 public:
  /// Smoothed values: dBFS (-100 when silent), or mapped linearly from their range to 0..1. Band 0 is the lowest.
  float get_rms_db() const { return this->analysis_.get_rms_db(); }
  float get_rms_level() const { return this->analysis_.get_rms_level(); }
  float get_band_db(uint8_t band) const { return this->analysis_.get_band_db(band); }
  float get_band_level(uint8_t band) const { return this->analysis_.get_band_level(band); }
  static constexpr uint8_t get_num_bands() { return NUM_BANDS; }
  bool is_active() const { return this->active_; }

  virtual void start_analysis() = 0;
  virtual void stop_analysis() = 0;

  /// Called in loop() after each analyzed frame.
  template<typename F> void add_on_frame_callback(F &&callback) {
    this->frame_callback_.add(std::forward<F>(callback));
  }

  void set_band_plan(float analysis_rate, const std::array<uint16_t, NUM_BANDS + 1> &band_bins) {
    this->analysis_.set_band_plan(analysis_rate, band_bins);
  }
  void set_smoothing(float attack, float release) { this->analysis_.set_smoothing(attack, release); }
  void set_rms_range(float min_db, float max_db) { this->analysis_.set_rms_range(min_db, max_db); }
  void set_band_range(float min_db, float max_db) { this->analysis_.set_band_range(min_db, max_db); }
  void set_decimation(uint8_t factor, const std::array<Decimator::Section, Decimator::NUM_SECTIONS> &sections) {
    this->tap_.get_decimator().configure(factor, sections);
  }
  /// In samples at the analysis rate.
  void set_buffer_samples(uint32_t samples) { this->buffer_samples_ = samples; }

 protected:
  void dump_analysis_config_();
  uint32_t buffer_ms_() const {
    return static_cast<uint32_t>(1000.0f * this->buffer_samples_ / this->analysis_.get_analysis_rate());
  }

  /// Applies a pending restart, then returns the number of buffered samples.
  uint32_t buffered_samples_();
  /// Analyzes the next ``due`` samples (as silence when ``silent``), skipping to the newest frame when far behind.
  void analyze_(uint32_t due, bool silent);
  /// Warning while the buffer overflows. Returns true when this call raised it.
  bool update_overflow_warning_(uint32_t now);
  void release_if_idle_(uint32_t now);
  bool is_idle_() const { return !this->active_ && this->analysis_.is_released(); }

  bool push_sample_(float sample);

  SpectrumAnalysis analysis_;
  StreamTap tap_;
  LazyCallbackManager<void()> frame_callback_;
  uint32_t buffer_samples_{0};
  /// Last frame, or last release step once idle
  uint32_t last_update_ms_{0};
  uint32_t last_overflow_ms_{0};
  bool active_{false};
};

}  // namespace esphome::audio_analyzer
