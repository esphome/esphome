#pragma once

// Platform-independent DSP of the audio analyzer: sample reading and downmix, anti-alias decimation, the FFT and the
// band/power maths. No ESP32, FreeRTOS or App dependency, so it runs in the host unit tests.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace esphome::audio_analyzer {

/// Samples per analysis frame (one FFT).
static constexpr uint16_t FFT_SIZE = 512;
/// Number of log-spaced frequency bands.
static constexpr uint8_t NUM_BANDS = 12;
/// Streams are decimated by an integer factor to at most this rate before the analysis.
static constexpr uint32_t MAX_ANALYSIS_RATE_HZ = 16000;
/// The highest band edge is at most this fraction of the analysis rate (the anti-alias filter's corner).
static constexpr float MAX_BAND_FRACTION = 0.4f;
/// Value of a silent power or band, in dBFS.
static constexpr float FLOOR_DB = -100.0f;

/// @brief Integer decimation factor that brings a stream to at most ``MAX_ANALYSIS_RATE_HZ`` (48 kHz -> 3).
inline uint8_t decimation_for(uint32_t sample_rate) {
  const uint32_t factor = (sample_rate + MAX_ANALYSIS_RATE_HZ - 1) / MAX_ANALYSIS_RATE_HZ;
  return static_cast<uint8_t>(factor < 1 ? 1 : factor);
}

/// @brief Reads one interleaved little-endian PCM frame and returns the mean of its channels in [-1, 1).
float read_mono_sample(const uint8_t *frame, uint8_t bytes_per_sample, uint8_t channels);

/// @brief Decimated samples due for analysis in a pass-through speaker.
///
/// The analysis buffer holds the newest copied audio, up to the write head: ``buffered_samples`` decimated samples,
/// ``decimation`` stream frames each. ``frames_in_output`` frames were written to the output speaker but not played
/// yet. Everything in the buffer older than those frames has been heard. Frames written while nothing was copied
/// are part of ``frames_in_output`` but not of the buffer, so they hold the analysis back until they are played.
inline uint32_t samples_heard(uint32_t buffered_samples, uint8_t decimation, uint32_t frames_in_output) {
  const uint64_t buffered_frames = static_cast<uint64_t>(buffered_samples) * decimation;
  if (buffered_frames <= frames_in_output) {
    return 0;
  }
  return static_cast<uint32_t>((buffered_frames - frames_in_output) / decimation);
}

/// @brief Anti-alias low-pass and integer decimation of a mono stream.
///
/// With a factor above 1, a 4th-order Butterworth low-pass (two biquads) at ``MAX_BAND_FRACTION`` of the output
/// rate runs on every input sample, then groups of ``factor`` samples are averaged. Content that would fold into the
/// analyzed range is attenuated by more than 30 dB (12 kHz into 4 kHz at 48 kHz).
class Decimator {
 public:
  /// @brief Sets the factor and filter for a stream rate and clears the filter state.
  void configure(uint32_t sample_rate);

  uint8_t get_factor() const { return this->factor_; }
  /// Input rate in Hz, 0 until configured.
  uint32_t get_input_rate() const { return this->input_rate_; }
  /// Output rate in Hz.
  float get_output_rate() const { return this->output_rate_; }

  /// @brief Feeds one input sample. Returns true and sets ``out`` when a decimated sample is ready.
  bool push(float in, float &out) {
    if (this->factor_ > 1) {
      in = this->sections_[1].run(this->sections_[0].run(in));
    }
    this->sum_ += in;
    if (++this->count_ < this->factor_) {
      return false;
    }
    out = this->sum_ * this->inv_factor_;
    this->sum_ = 0.0f;
    this->count_ = 0;
    return true;
  }

 protected:
  /// Transposed direct form II biquad, coefficients normalized by a0.
  struct Biquad {
    float b0{1.0f}, b1{0.0f}, b2{0.0f}, a1{0.0f}, a2{0.0f};
    float z1{0.0f}, z2{0.0f};
    float run(float x) {
      const float y = this->b0 * x + this->z1;
      this->z1 = this->b1 * x - this->a1 * y + this->z2;
      this->z2 = this->b2 * x - this->a2 * y;
      return y;
    }
  };

  std::array<Biquad, 2> sections_{};
  float output_rate_{static_cast<float>(MAX_ANALYSIS_RATE_HZ)};
  float inv_factor_{1.0f};
  float sum_{0.0f};
  uint32_t input_rate_{0};
  uint8_t factor_{1};
  uint8_t count_{0};
};

/// @brief Power and log-spaced band spectrum of a mono stream at the analysis rate.
///
/// Samples are collected into frames of ``FFT_SIZE``. Each full frame gives:
///   - power: mean square of the frame in dBFS (the same scale as ``sound_level``'s RMS sensor)
///   - ``NUM_BANDS`` bands: Hann-windowed FFT, one-sided bin powers summed into log-spaced bands between the minimum
///     and maximum frequency, scaled so the bands of a broadband signal add up to its power (Parseval)
/// Power and bands are smoothed in dB with the attack and release time constants; the 0..1 levels map the smoothed
/// dB values linearly from their range. Without frames, ``release()`` moves them towards ``FLOOR_DB``.
class SpectrumAnalysis {
 public:
  void set_frequency_range(float min_hz, float max_hz) {
    this->min_frequency_ = min_hz;
    this->max_frequency_ = max_hz;
  }
  void set_power_range(float min_db, float max_db) {
    this->power_min_db_ = min_db;
    this->power_max_db_ = max_db;
  }
  void set_band_range(float min_db, float max_db) {
    this->band_min_db_ = min_db;
    this->band_max_db_ = max_db;
  }
  void set_attack(uint32_t attack_ms) { this->attack_ms_ = attack_ms; }
  void set_release(uint32_t release_ms) { this->release_ms_ = release_ms; }

  /// @brief Computes the band plan for an analysis rate and drops the partial frame. Returns false when the maximum
  /// frequency was lowered to ``MAX_BAND_FRACTION`` of the rate.
  bool configure(float analysis_rate);

  /// @brief Adds one sample at the analysis rate. Returns true when it completed and analyzed a frame.
  bool push(float sample);

  /// @brief Drops the partial frame.
  void reset_frame() { this->frame_fill_ = 0; }

  /// @brief Moves the outputs towards silence as if ``seconds`` of silence were analyzed.
  void release(float seconds);
  /// True once the outputs reached silence.
  bool is_released() const;

  float get_power_db() const { return this->power_db_; }
  float get_power_level() const { return normalize(this->power_db_, this->power_min_db_, this->power_max_db_); }
  float get_band_db(uint8_t band) const { return band < NUM_BANDS ? this->band_db_[band] : FLOOR_DB; }
  float get_band_level(uint8_t band) const {
    return normalize(this->get_band_db(band), this->band_min_db_, this->band_max_db_);
  }

  float get_analysis_rate() const { return this->analysis_rate_; }
  /// Duration of one analysis frame in seconds.
  float get_frame_seconds() const { return static_cast<float>(FFT_SIZE) / this->analysis_rate_; }
  /// Upper edge of the band plan in Hz, after any Nyquist clamp.
  float get_upper_frequency() const { return this->upper_frequency_; }
  /// First FFT bin of each band; entry ``NUM_BANDS`` is the exclusive end of the last band.
  const std::array<uint16_t, NUM_BANDS + 1> &get_band_bins() const { return this->band_bins_; }

  float get_min_frequency() const { return this->min_frequency_; }
  float get_max_frequency() const { return this->max_frequency_; }
  float get_power_min_db() const { return this->power_min_db_; }
  float get_power_max_db() const { return this->power_max_db_; }
  float get_band_min_db() const { return this->band_min_db_; }
  float get_band_max_db() const { return this->band_max_db_; }
  uint32_t get_attack() const { return this->attack_ms_; }
  uint32_t get_release() const { return this->release_ms_; }

  static float normalize(float value, float min_value, float max_value) {
    const float level = (value - min_value) / (max_value - min_value);
    return level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
  }

 protected:
  void analyze_frame_();
  void apply_targets_(float power_db, const float *band_db, float seconds);

  // FFT working set, 4 KB. The window and twiddle factors are constant tables in flash.
  std::array<float, FFT_SIZE> re_{};
  std::array<float, FFT_SIZE> im_{};

  std::array<float, NUM_BANDS> band_db_{FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB,
                                        FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB};
  std::array<uint16_t, NUM_BANDS + 1> band_bins_{};

  float analysis_rate_{static_cast<float>(MAX_ANALYSIS_RATE_HZ)};
  float upper_frequency_{4000.0f};
  float power_db_{FLOOR_DB};

  float min_frequency_{100.0f};
  float max_frequency_{4000.0f};
  float power_min_db_{-70.0f};
  float power_max_db_{-20.0f};
  float band_min_db_{-80.0f};
  float band_max_db_{-30.0f};
  uint32_t attack_ms_{30};
  uint32_t release_ms_{250};

  uint16_t frame_fill_{0};
};

}  // namespace esphome::audio_analyzer
