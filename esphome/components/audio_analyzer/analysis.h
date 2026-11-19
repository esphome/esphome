#pragma once

// Platform-independent DSP of the audio analyzer: sample reading and downmix, anti-alias decimation, the FFT and the
// band/RMS maths. No ESP32, FreeRTOS or App dependency, so it runs in the host unit tests.

#include <array>
#include <cstdint>

namespace esphome::audio_analyzer {

/// Samples per analysis frame (one FFT).
static constexpr uint16_t FFT_SIZE = 512;
/// Number of log-spaced frequency bands.
static constexpr uint8_t NUM_BANDS = 12;
/// Streams are decimated by an integer factor to at most this rate before the analysis.
static constexpr uint32_t MAX_ANALYSIS_RATE_HZ = 16000;
/// The highest band edge is at most this fraction of the analysis rate. Kept well below the anti-alias corner
/// (``FILTER_CORNER_FRACTION``), so the bands sit in the filter's flat passband and away from its transition band.
static constexpr float MAX_BAND_FRACTION = 0.3f;
/// Corner of the anti-alias low-pass as a fraction of the analysis rate.
static constexpr float FILTER_CORNER_FRACTION = 0.4f;
/// Narrowest band in FFT bins: the 6 dB width of the Hann window's main lobe, so a tone inside a band is mostly
/// measured by that band.
static constexpr uint8_t MIN_BAND_BINS = 2;
/// Value of a silent RMS or band, in dBFS.
static constexpr float FLOOR_DB = -100.0f;

/// @brief Integer decimation factor that brings a stream to at most ``MAX_ANALYSIS_RATE_HZ`` (48 kHz -> 3).
inline uint8_t decimation_for(uint32_t sample_rate) {
  const uint32_t factor = (sample_rate + MAX_ANALYSIS_RATE_HZ - 1) / MAX_ANALYSIS_RATE_HZ;
  return static_cast<uint8_t>(factor < 1 ? 1 : factor);
}

/// @brief Reads one interleaved little-endian PCM frame and returns the mean of its channels in [-1, 1).
inline float read_mono_sample(const uint8_t *frame, uint8_t bytes_per_sample, uint8_t channels) {
  float sum = 0.0f;
  for (uint8_t c = 0; c < channels; ++c) {
    const uint8_t *p = frame + c * bytes_per_sample;
    switch (bytes_per_sample) {
      case 1:
        sum += static_cast<float>(static_cast<int8_t>(p[0])) * (1.0f / 128.0f);
        break;
      case 2:
        sum += static_cast<float>(static_cast<int16_t>(p[0] | (p[1] << 8))) * (1.0f / 32768.0f);
        break;
      case 3:
        sum += static_cast<float>(static_cast<int32_t>((static_cast<uint32_t>(p[0]) << 8) |
                                                       (static_cast<uint32_t>(p[1]) << 16) |
                                                       (static_cast<uint32_t>(p[2]) << 24))) *
               (1.0f / 2147483648.0f);
        break;
      case 4:
        sum += static_cast<float>(
                   static_cast<int32_t>(static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
                                        (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24))) *
               (1.0f / 2147483648.0f);
        break;
      default:
        return 0.0f;
    }
  }
  // Mono and stereo without a division; any other count is rare
  if (channels == 2) {
    return sum * 0.5f;
  }
  return channels > 2 ? sum / static_cast<float>(channels) : sum;
}

/// @brief Anti-alias low-pass and integer decimation of a mono stream.
///
/// With a factor above 1, an 8th-order Butterworth low-pass (four biquads, corner at ``FILTER_CORNER_FRACTION`` of
/// the output rate) runs on every input sample and every ``factor``-th filtered sample is kept. Up to the highest
/// band edge (``MAX_BAND_FRACTION``) the passband loses less than 0.1 dB, so the levels match those of a stream that
/// needs no decimation. Content that folds into 0 Hz to the highest band edge is attenuated by more than 45 dB for
/// streams up to 48 kHz.
class Decimator {
 public:
  /// @brief Sets the factor and filter for a stream rate and clears the filter state.
  void configure(uint32_t sample_rate);
  /// @brief Clears the filter state, so a new stream doesn't start with the end of the previous one.
  void reset();

  uint8_t get_factor() const { return this->factor_; }
  /// Input rate in Hz, 0 until configured.
  uint32_t get_input_rate() const { return this->input_rate_; }
  /// Output rate in Hz.
  float get_output_rate() const { return this->output_rate_; }

  /// @brief Feeds one input sample. Returns true and sets ``out`` when a decimated sample is ready.
  bool push(float in, float &out) {
    if (this->factor_ == 1) {
      out = in;
      return true;
    }
    for (Biquad &section : this->sections_) {
      in = section.run(in);
    }
    if (++this->count_ < this->factor_) {
      return false;
    }
    this->count_ = 0;
    out = in;
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

  std::array<Biquad, 4> sections_{};
  float output_rate_{static_cast<float>(MAX_ANALYSIS_RATE_HZ)};
  uint32_t input_rate_{0};
  uint8_t factor_{1};
  uint8_t count_{0};
};

/// @brief RMS level and log-spaced band spectrum of a mono stream at the analysis rate.
///
/// Samples are collected into frames of ``FFT_SIZE``. Each full frame gives:
///   - RMS: root mean square of the frame in dBFS (the scale of ``sound_level``'s RMS sensor). DC counts.
///   - ``NUM_BANDS`` bands: Hann-windowed FFT, one-sided bin powers summed into log-spaced bands between the minimum
///     and maximum frequency, scaled so the bands of a broadband signal add up to its mean square (Parseval). DC is
///     never in a band. Each band is at least ``MIN_BAND_BINS`` bins wide; where the logarithmic spacing would be
///     narrower (the lowest bands), the bands are ``MIN_BAND_BINS`` wide.
/// RMS and bands are smoothed in dB with the attack and release time constants; the 0..1 levels map the smoothed
/// dB values linearly from their range. Without frames, ``release()`` moves them towards ``FLOOR_DB``.
class SpectrumAnalysis {
 public:
  void set_frequency_range(float min_hz, float max_hz) {
    this->min_frequency_ = min_hz;
    this->max_frequency_ = max_hz;
  }
  void set_rms_range(float min_db, float max_db) {
    this->rms_min_db_ = min_db;
    this->rms_max_db_ = max_db;
  }
  void set_band_range(float min_db, float max_db) {
    this->band_min_db_ = min_db;
    this->band_max_db_ = max_db;
  }
  void set_attack(uint32_t attack_ms) { this->attack_ms_ = attack_ms; }
  void set_release(uint32_t release_ms) { this->release_ms_ = release_ms; }

  /// @brief Computes the band plan for an analysis rate and drops the partial frame. Returns false when the
  /// configured range did not fit the rate: the top was lowered to ``MAX_BAND_FRACTION`` of the rate, and the bottom
  /// too if the bands no longer had room.
  bool configure(float analysis_rate);

  /// @brief Adds one sample at the analysis rate. Returns true when it completed and analyzed a frame.
  bool push(float sample);

  /// @brief Drops the partial frame.
  void reset_frame() { this->frame_fill_ = 0; }

  /// @brief Moves the outputs towards silence as if ``seconds`` of silence were analyzed.
  void release(float seconds);
  /// True once the outputs reached silence.
  bool is_released() const;

  float get_rms_db() const { return this->rms_db_; }
  float get_rms_level() const { return normalize(this->rms_db_, this->rms_min_db_, this->rms_max_db_); }
  float get_band_db(uint8_t band) const { return band < NUM_BANDS ? this->band_db_[band] : FLOOR_DB; }
  float get_band_level(uint8_t band) const {
    return normalize(this->get_band_db(band), this->band_min_db_, this->band_max_db_);
  }

  float get_analysis_rate() const { return this->analysis_rate_; }
  /// Duration of one analysis frame in seconds.
  float get_frame_seconds() const { return static_cast<float>(FFT_SIZE) / this->analysis_rate_; }
  /// FFT bin width in Hz.
  float get_bin_width() const { return this->analysis_rate_ / FFT_SIZE; }
  /// Lower edge of band 0 in Hz, as analyzed (rounded to bins, after any clamp).
  float get_lower_edge() const { return (this->band_bins_[0] - 0.5f) * this->get_bin_width(); }
  /// Upper edge of the last band in Hz, as analyzed (rounded to bins, after any clamp).
  float get_upper_edge() const { return (this->band_bins_[NUM_BANDS] - 0.5f) * this->get_bin_width(); }
  /// First FFT bin of each band; entry ``NUM_BANDS`` is the exclusive end of the last band.
  const std::array<uint16_t, NUM_BANDS + 1> &get_band_bins() const { return this->band_bins_; }

  float get_min_frequency() const { return this->min_frequency_; }
  float get_max_frequency() const { return this->max_frequency_; }
  float get_rms_min_db() const { return this->rms_min_db_; }
  float get_rms_max_db() const { return this->rms_max_db_; }
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
  void apply_targets_(float rms_db, const float *band_db, float seconds);

  // FFT working set, 4 KB. The window and twiddle factors are constant tables in flash.
  std::array<float, FFT_SIZE> re_{};
  std::array<float, FFT_SIZE> im_{};

  std::array<float, NUM_BANDS> band_db_{FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB,
                                        FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB, FLOOR_DB};
  std::array<uint16_t, NUM_BANDS + 1> band_bins_{};

  float analysis_rate_{static_cast<float>(MAX_ANALYSIS_RATE_HZ)};
  float rms_db_{FLOOR_DB};

  float min_frequency_{100.0f};
  float max_frequency_{4000.0f};
  float rms_min_db_{-70.0f};
  float rms_max_db_{-20.0f};
  float band_min_db_{-80.0f};
  float band_max_db_{-30.0f};
  uint32_t attack_ms_{30};
  uint32_t release_ms_{250};

  uint16_t frame_fill_{0};
};

}  // namespace esphome::audio_analyzer
