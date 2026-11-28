#pragma once

// Platform-independent DSP, run by the host unit tests. Code generation works out the band plan, filter and
// smoothing factors.

#include "esphome/components/audio/audio.h"

#include <array>
#include <cstdint>

namespace esphome::audio_analyzer {

// FFT_SIZE and NUM_BANDS must match __init__.py
static constexpr uint16_t FFT_SIZE = 512;
static constexpr uint8_t NUM_BANDS = 12;
static constexpr float FLOOR_DB = -100.0f;

/// Mean of the channels of one little-endian PCM frame, in [-1, 1).
inline float read_mono_sample(const uint8_t *frame, uint8_t bytes_per_sample, uint8_t channels) {
  static constexpr float Q31_SCALE = 1.0f / 2147483648.0f;
  float sum = 0.0f;
  for (uint8_t c = 0; c < channels; ++c) {
    sum += static_cast<float>(audio::unpack_audio_sample_to_q31(frame + c * bytes_per_sample, bytes_per_sample));
  }
  if (channels == 2) {
    return sum * (0.5f * Q31_SCALE);
  }
  return channels > 2 ? sum * Q31_SCALE / static_cast<float>(channels) : sum * Q31_SCALE;
}

/// Anti-alias low-pass (four biquads) and integer decimation of a mono stream.
class Decimator {
 public:
  static constexpr uint8_t NUM_SECTIONS = 4;
  /// Normalized by a0; the numerator is b0 (1 + 2 z^-1 + z^-2).
  struct Section {
    float b0, a1, a2;
  };

  void configure(uint8_t factor, const std::array<Section, NUM_SECTIONS> &sections) {
    this->factor_ = factor;
    this->sections_ = sections;
    this->reset();
  }
  void reset() {
    this->state_ = {};
    this->count_ = 0;
  }

  uint8_t get_factor() const { return this->factor_; }

  /// Returns true and sets ``out`` when a decimated sample is ready.
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

/// RMS (dBFS, as ``sound_level``) and band powers of each frame of ``FFT_SIZE`` samples, smoothed in dB.
/// Bands sum the Hann-windowed FFT bins of the band plan, scaled so they add up to the mean square (Parseval).
class SpectrumAnalysis {
 public:
  /// First FFT bin of each band; entry ``NUM_BANDS`` ends the last band.
  void set_band_plan(float analysis_rate, const std::array<uint16_t, NUM_BANDS + 1> &band_bins) {
    this->analysis_rate_ = analysis_rate;
    this->band_bins_ = band_bins;
    this->frame_fill_ = 0;
  }
  /// Share of the step to a louder (attack) or quieter (release) frame applied per frame, 1 for none.
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

  /// Returns true when the sample completed a frame.
  bool push(float sample);

  void reset_frame() { this->frame_fill_ = 0; }

  /// Smooths towards silence as one silent frame would.
  void release_step();
  bool is_released() const;

  float get_rms_db() const { return this->rms_db_; }
  float get_rms_level() const { return normalize(this->rms_db_, this->rms_min_db_, this->rms_max_db_); }
  float get_band_db(uint8_t band) const { return band < NUM_BANDS ? this->band_db_[band] : FLOOR_DB; }
  float get_band_level(uint8_t band) const {
    return normalize(this->get_band_db(band), this->band_min_db_, this->band_max_db_);
  }

  float get_analysis_rate() const { return this->analysis_rate_; }
  uint32_t get_frame_ms() const { return static_cast<uint32_t>(1000.0f * FFT_SIZE / this->analysis_rate_); }
  float get_lower_edge() const { return (this->band_bins_[0] - 0.5f) * this->analysis_rate_ / FFT_SIZE; }
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

  // FFT working set, 4 KB
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
