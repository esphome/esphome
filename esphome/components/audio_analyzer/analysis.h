#pragma once

// Platform-independent DSP of the audio analyzer: sample reading and downmix, anti-alias decimation, the FFT and the
// band/RMS maths. No ESP32, FreeRTOS or App dependency, so it runs in the host unit tests. Everything that only
// depends on the configuration (band plan, filter coefficients, smoothing factors) is worked out by code generation
// (__init__.py) and passed in.

#include "esphome/components/audio/audio.h"

#include <array>
#include <cstdint>

namespace esphome::audio_analyzer {

/// Samples per analysis frame (one FFT). Code generation checks that __init__.py uses the same value.
static constexpr uint16_t FFT_SIZE = 512;
/// Number of log-spaced frequency bands. Code generation checks that __init__.py uses the same value.
static constexpr uint8_t NUM_BANDS = 12;
/// Value of a silent RMS or band, in dBFS.
static constexpr float FLOOR_DB = -100.0f;

/// @brief Reads one interleaved little-endian PCM frame and returns the mean of its channels in [-1, 1).
inline float read_mono_sample(const uint8_t *frame, uint8_t bytes_per_sample, uint8_t channels) {
  static constexpr float Q31_SCALE = 1.0f / 2147483648.0f;
  float sum = 0.0f;
  for (uint8_t c = 0; c < channels; ++c) {
    sum += static_cast<float>(audio::unpack_audio_sample_to_q31(frame + c * bytes_per_sample, bytes_per_sample));
  }
  // Mono and stereo without a division; any other count is rare
  if (channels == 2) {
    return sum * (0.5f * Q31_SCALE);
  }
  return channels > 2 ? sum * Q31_SCALE / static_cast<float>(channels) : sum * Q31_SCALE;
}

/// @brief Anti-alias low-pass and integer decimation of a mono stream.
///
/// With a factor above 1, an 8th-order low-pass (four biquad sections) runs on every input sample and every
/// ``factor``-th filtered sample is kept. Code generation designs the filter for the configured stream rate.
class Decimator {
 public:
  static constexpr uint8_t NUM_SECTIONS = 4;
  /// Low-pass biquad section normalized by a0. The numerator is b0 (1 + 2 z^-1 + z^-2).
  struct Section {
    float b0, a1, a2;
  };

  /// @brief Sets the decimation factor and its filter. Without it, the stream passes unchanged.
  void configure(uint8_t factor, const std::array<Section, NUM_SECTIONS> &sections) {
    this->factor_ = factor;
    this->sections_ = sections;
    this->reset();
  }
  /// @brief Clears the filter state, so a new stream doesn't start with the end of the previous one.
  void reset() {
    this->state_ = {};
    this->count_ = 0;
  }

  uint8_t get_factor() const { return this->factor_; }

  /// @brief Feeds one input sample. Returns true and sets ``out`` when a decimated sample is ready.
  bool push(float in, float &out) {
    if (this->factor_ == 1) {
      out = in;
      return true;
    }
    // Transposed direct form II
    for (uint8_t s = 0; s < NUM_SECTIONS; ++s) {
      const Section &c = this->sections_[s];
      State &z = this->state_[s];
      const float y = c.b0 * in + z.z1;
      z.z1 = 2.0f * c.b0 * in - c.a1 * y + z.z2;
      z.z2 = c.b0 * in - c.a2 * y;
      in = y;
    }
    if (++this->count_ < this->factor_) {
      return false;
    }
    this->count_ = 0;
    out = in;
    return true;
  }

 protected:
  struct State {
    float z1, z2;
  };
  std::array<Section, NUM_SECTIONS> sections_{};
  std::array<State, NUM_SECTIONS> state_{};
  uint8_t factor_{1};
  uint8_t count_{0};
};

/// @brief RMS level and log-spaced band spectrum of a mono stream at the analysis rate.
///
/// Samples are collected into frames of ``FFT_SIZE``. Each full frame gives:
///   - RMS: root mean square of the frame in dBFS (the dBFS scale of ``sound_level``). DC counts.
///   - ``NUM_BANDS`` bands: Hann-windowed FFT, one-sided bin powers summed over each band's bins (the band plan),
///     scaled so the bands of a broadband signal add up to its mean square (Parseval).
/// RMS and bands are smoothed in dB, one step per frame, with the attack and release factors; the 0..1 levels map the
/// smoothed dB values linearly from their range. Without frames, ``release_step()`` moves them towards ``FLOOR_DB``.
class SpectrumAnalysis {
 public:
  /// @brief Sets the analysis rate and the first FFT bin of each band (entry ``NUM_BANDS`` is the end of the last
  /// band). Drops the partial frame.
  void set_band_plan(float analysis_rate, const std::array<uint16_t, NUM_BANDS + 1> &band_bins) {
    this->analysis_rate_ = analysis_rate;
    this->band_bins_ = band_bins;
    this->frame_fill_ = 0;
  }
  /// @brief Sets the smoothing factors per frame: the share of the step to a louder (attack) or quieter (release)
  /// frame applied at once, 1 for none.
  void set_smoothing(float attack, float release) {
    this->attack_ = attack;
    this->release_ = release;
  }
  void set_rms_range(float min_db, float max_db) {
    this->rms_min_db_ = min_db;
    this->rms_max_db_ = max_db;
  }
  void set_band_range(float min_db, float max_db) {
    this->band_min_db_ = min_db;
    this->band_max_db_ = max_db;
  }

  /// @brief Adds one sample at the analysis rate. Returns true when it completed and analyzed a frame.
  bool push(float sample);

  /// @brief Drops the partial frame.
  void reset_frame() { this->frame_fill_ = 0; }

  /// @brief Moves the outputs towards silence as if one frame of silence was analyzed.
  void release_step();
  /// True once the outputs reached silence.
  bool is_released() const;

  float get_rms_db() const { return this->rms_db_; }
  float get_rms_level() const { return normalize(this->rms_db_, this->rms_min_db_, this->rms_max_db_); }
  float get_band_db(uint8_t band) const { return band < NUM_BANDS ? this->band_db_[band] : FLOOR_DB; }
  float get_band_level(uint8_t band) const {
    return normalize(this->get_band_db(band), this->band_min_db_, this->band_max_db_);
  }

  float get_analysis_rate() const { return this->analysis_rate_; }
  /// Duration of one analysis frame in milliseconds, rounded down.
  uint32_t get_frame_ms() const { return static_cast<uint32_t>(1000.0f * FFT_SIZE / this->analysis_rate_); }
  /// Lower edge of band 0 in Hz.
  float get_lower_edge() const { return (this->band_bins_[0] - 0.5f) * this->analysis_rate_ / FFT_SIZE; }
  /// Upper edge of the last band in Hz.
  float get_upper_edge() const { return (this->band_bins_[NUM_BANDS] - 0.5f) * this->analysis_rate_ / FFT_SIZE; }

  float get_rms_min_db() const { return this->rms_min_db_; }
  float get_rms_max_db() const { return this->rms_max_db_; }
  float get_band_min_db() const { return this->band_min_db_; }
  float get_band_max_db() const { return this->band_max_db_; }

  static float normalize(float value, float min_value, float max_value) {
    const float level = (value - min_value) / (max_value - min_value);
    return level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
  }

 protected:
  void analyze_frame_();
  void apply_targets_(float rms_db, const float *band_db);

  // FFT working set, 4 KB. The window and twiddle factors are constant tables in flash.
  std::array<float, FFT_SIZE> re_{};
  std::array<float, FFT_SIZE> im_{};

  std::array<float, NUM_BANDS> band_db_{FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB,
                                        FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB};
  std::array<uint16_t, NUM_BANDS + 1> band_bins_{};

  float analysis_rate_{16000.0f};
  float rms_db_{FLOOR_DB};
  float attack_{1.0f};
  float release_{1.0f};
  float rms_min_db_{-70.0f};
  float rms_max_db_{-20.0f};
  float band_min_db_{-80.0f};
  float band_max_db_{-30.0f};

  uint16_t frame_fill_{0};
};

}  // namespace esphome::audio_analyzer
