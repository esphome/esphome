#pragma once

#include "analysis.h"
#include "stream_tap.h"

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <array>
#include <utility>

namespace esphome::audio_analyzer {

/// @brief Live RMS level and spectrum of an audio stream, for visualizers, level meters and light effects.
///
/// Audio arrives on another task, which only copies it (``StreamTap``). The analysis (``SpectrumAnalysis``) runs in
/// ``loop()`` on the main task, so the getters can be called from lambdas (light effects, templates) without locking.
/// Streams are downmixed to mono and decimated to at most 16 kHz, then analyzed in frames of 512 samples (32 ms at
/// 16 kHz).
///
/// The getters always return a number: without audio the values release towards silence (-100 dBFS, level 0) and
/// ``is_active()`` turns false.
///
/// The platform-independent part of both analyzers: the platforms only feed the tap and decide how much of it is due.
class AudioAnalyzer : public Component {
 public:
  /// Smoothed RMS level in dBFS (the dBFS scale of sound_level), -100 when silent.
  float get_rms_db() const { return this->analysis_.get_rms_db(); }
  /// Smoothed RMS level mapped linearly from the RMS range to 0..1.
  float get_rms_level() const { return this->analysis_.get_rms_level(); }
  /// Smoothed power of a band in dBFS, -100 when silent. Band 0 is the lowest frequency.
  float get_band_db(uint8_t band) const { return this->analysis_.get_band_db(band); }
  /// Smoothed power of a band mapped linearly from the band range to 0..1.
  float get_band_level(uint8_t band) const { return this->analysis_.get_band_level(band); }
  static constexpr uint8_t get_num_bands() { return NUM_BANDS; }
  /// True while audio is being analyzed.
  bool is_active() const { return this->active_; }

  /// @brief Starts the analysis: the microphone analyzer starts its microphone, the speaker copies its audio again.
  virtual void start_analysis() = 0;
  /// @brief Stops the analysis; the values release to silence.
  virtual void stop_analysis() = 0;

  /// @brief Registers a callback called on the main task after each analyzed frame.
  template<typename F> void add_on_frame_callback(F &&callback) {
    this->frame_callback_.add(std::forward<F>(callback));
  }

  // Worked out by code generation from the configuration and the stream's sample rate
  void set_band_plan(float analysis_rate, const std::array<uint16_t, NUM_BANDS + 1> &band_bins) {
    this->analysis_.set_band_plan(analysis_rate, band_bins);
  }
  void set_smoothing(float attack, float release) { this->analysis_.set_smoothing(attack, release); }
  void set_rms_range(float min_db, float max_db) { this->analysis_.set_rms_range(min_db, max_db); }
  void set_band_range(float min_db, float max_db) { this->analysis_.set_band_range(min_db, max_db); }
  void set_decimation(uint8_t factor, const std::array<Decimator::Section, Decimator::NUM_SECTIONS> &sections) {
    this->tap_.get_decimator().configure(factor, sections);
  }
  /// Capacity of the buffer between the audio task and loop(), in samples at the analysis rate.
  void set_buffer_samples(uint32_t samples) { this->buffer_samples_ = samples; }

 protected:
  void dump_analysis_config_();
  /// Duration of the buffer in ms.
  uint32_t buffer_ms_() const {
    return static_cast<uint32_t>(1000.0f * this->buffer_samples_ / this->analysis_.get_analysis_rate());
  }

  /// @brief Empties the buffer when a restart is pending (dropping the partial frame too), then returns the number of
  /// buffered samples. Call it before working out how many are due.
  uint32_t buffered_samples_();
  /// @brief Analyzes the next ``due`` buffered samples, as silence when ``silent``. When more than two frames are due
  /// (the loop was blocked), skips to the newest frame instead of catching up.
  void analyze_(uint32_t due, bool silent);
  /// @brief Sets the warning status when the buffer was full since the last call, clears it after a second without.
  /// Returns true when the warning was raised by this call, so the platform can log why.
  bool update_overflow_warning_(uint32_t now);
  /// @brief Releases the outputs towards silence, one frame of silence per frame period without audio.
  void release_if_idle_(uint32_t now);
  bool is_idle_() const { return !this->active_ && this->analysis_.is_released(); }

  bool push_sample_(float sample);

  SpectrumAnalysis analysis_;
  StreamTap tap_;
  LazyCallbackManager<void()> frame_callback_;
  uint32_t buffer_samples_{0};
  /// Time of the last frame, or of the last release step once idle
  uint32_t last_update_ms_{0};
  uint32_t last_overflow_ms_{0};
  bool active_{false};
};

}  // namespace esphome::audio_analyzer
