#include "analysis.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace esphome::audio_analyzer {

static constexpr double PI = std::numbers::pi;

// cos(x) for x in [-pi, pi], Taylor series in double, evaluated at compile time
static constexpr double constexpr_cos(double x) {
  double term = 1.0;
  double sum = 1.0;
  for (int n = 1; n < 30; ++n) {
    term *= -x * x / ((2 * n - 1) * (2 * n));
    sum += term;
  }
  return sum;
}

static constexpr std::array<float, FFT_SIZE / 2 + 1> make_cos_table() {
  std::array<float, FFT_SIZE / 2 + 1> table{};
  for (uint16_t k = 0; k <= FFT_SIZE / 2; ++k) {
    table[k] = static_cast<float>(constexpr_cos(2.0 * PI * k / FFT_SIZE));
  }
  return table;
}

// cos(2 pi k / FFT_SIZE) for k in [0, FFT_SIZE / 2]: the FFT twiddle factors and the Hann window, in flash
static constexpr std::array<float, FFT_SIZE / 2 + 1> COS_TABLE = make_cos_table();
// Sum of the squared periodic Hann window over one frame (3 N / 8)
static constexpr float WINDOW_POWER_SUM = 3.0f * FFT_SIZE / 8.0f;
static constexpr float FLOOR_POWER = 1e-10f;  // FLOOR_DB

static inline float to_db(float power) { return 10.0f * log10f(std::max(power, FLOOR_POWER)); }

static inline float hann(uint16_t i) {
  // cos(2 pi i / N) = cos(2 pi (N - i) / N)
  return 0.5f - 0.5f * COS_TABLE[i <= FFT_SIZE / 2 ? i : FFT_SIZE - i];
}

static inline float smoothing_alpha(float seconds, uint32_t time_constant_ms) {
  if (time_constant_ms == 0) {
    return 1.0f;
  }
  return 1.0f - expf(-seconds * 1000.0f / static_cast<float>(time_constant_ms));
}

void Decimator::configure(uint32_t sample_rate) {
  this->input_rate_ = sample_rate;
  this->factor_ = decimation_for(sample_rate);
  this->output_rate_ = static_cast<float>(sample_rate) / this->factor_;

  // 8th-order Butterworth low-pass as four biquads (the Q of each pole pair), corner at FILTER_CORNER_FRACTION of
  // the output rate (RBJ cookbook coefficients)
  static constexpr double SECTION_Q[4] = {0.50979557910415917, 0.60134488693504528, 0.89997622313641570,
                                          2.5629154477415055};
  const double w0 = 2.0 * PI * FILTER_CORNER_FRACTION / this->factor_;
  const double cos_w0 = std::cos(w0);
  for (size_t s = 0; s < this->sections_.size(); ++s) {
    const double alpha = std::sin(w0) / (2.0 * SECTION_Q[s]);
    const double a0 = 1.0 + alpha;
    Biquad &section = this->sections_[s];
    section.b0 = static_cast<float>((1.0 - cos_w0) / 2.0 / a0);
    section.b1 = static_cast<float>((1.0 - cos_w0) / a0);
    section.b2 = section.b0;
    section.a1 = static_cast<float>(-2.0 * cos_w0 / a0);
    section.a2 = static_cast<float>((1.0 - alpha) / a0);
  }
  this->reset();
}

void Decimator::reset() {
  for (Biquad &section : this->sections_) {
    section.z1 = 0.0f;
    section.z2 = 0.0f;
  }
  this->count_ = 0;
}

bool SpectrumAnalysis::configure(float analysis_rate) {
  this->analysis_rate_ = analysis_rate;
  this->frame_fill_ = 0;

  const float bin_width = analysis_rate / FFT_SIZE;
  const float limit = MAX_BAND_FRACTION * analysis_rate;
  bool fits = this->max_frequency_ <= limit;
  const float upper_hz = fits ? this->max_frequency_ : limit;
  const auto to_bin = [bin_width](float hz) { return static_cast<int32_t>(lroundf(hz / bin_width)); };

  // Every band gets at least MIN_BAND_BINS bins. When the clamped top leaves no room for that, the bottom moves down.
  static constexpr int32_t MIN_SPAN = NUM_BANDS * MIN_BAND_BINS;
  const int32_t upper_bin = std::max(to_bin(upper_hz), MIN_SPAN + 1);
  int32_t lower_bin = std::max(to_bin(this->min_frequency_), static_cast<int32_t>(1));
  if (upper_bin - lower_bin < MIN_SPAN) {
    lower_bin = upper_bin - MIN_SPAN;
    fits = false;
  }

  // Log-spaced edges between the two bins, widened from the bottom so no band is narrower than MIN_BAND_BINS, then
  // pulled back from the top so the last band ends at upper_bin. The span leaves room for both passes.
  const float lower_hz = lower_bin * bin_width;
  const float ratio = (upper_bin * bin_width) / lower_hz;
  std::array<int32_t, NUM_BANDS + 1> bins{};
  bins[0] = lower_bin;
  for (uint8_t b = 1; b < NUM_BANDS; ++b) {
    bins[b] = std::max(to_bin(lower_hz * powf(ratio, static_cast<float>(b) / NUM_BANDS)), bins[b - 1] + MIN_BAND_BINS);
  }
  bins[NUM_BANDS] = upper_bin;
  for (int8_t b = NUM_BANDS - 1; b > 0; --b) {
    bins[b] = std::min(bins[b], bins[b + 1] - MIN_BAND_BINS);
  }
  for (uint8_t b = 0; b <= NUM_BANDS; ++b) {
    this->band_bins_[b] = static_cast<uint16_t>(bins[b]);
  }
  return fits;
}

bool SpectrumAnalysis::push(float sample) {
  this->re_[this->frame_fill_++] = sample;
  if (this->frame_fill_ < FFT_SIZE) {
    return false;
  }
  this->frame_fill_ = 0;
  this->analyze_frame_();
  return true;
}

void SpectrumAnalysis::analyze_frame_() {
  float *re = this->re_.data();
  float *im = this->im_.data();

  float squared_sum = 0.0f;
  for (uint16_t i = 0; i < FFT_SIZE; ++i) {
    squared_sum += re[i] * re[i];
    re[i] *= hann(i);
    im[i] = 0.0f;
  }

  // In-place iterative radix-2 FFT
  for (uint16_t i = 1, j = 0; i < FFT_SIZE; ++i) {
    uint16_t bit = FFT_SIZE >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      std::swap(re[i], re[j]);
    }
  }
  for (uint16_t len = 2; len <= FFT_SIZE; len <<= 1) {
    const uint16_t half = len >> 1;
    const uint16_t step = FFT_SIZE / len;
    for (uint16_t k = 0; k < half; ++k) {
      // sin(2 pi k step / N) = cos(2 pi (N / 4 - k step) / N), and the table is even around 0
      const uint16_t index = k * step;
      const float c = COS_TABLE[index];
      const float s = COS_TABLE[index <= FFT_SIZE / 4 ? FFT_SIZE / 4 - index : index - FFT_SIZE / 4];
      for (uint16_t a = k; a < FFT_SIZE; a += len) {
        const uint16_t b = a + half;
        const float tr = re[b] * c + im[b] * s;
        const float ti = im[b] * c - re[b] * s;
        re[b] = re[a] - tr;
        im[b] = im[a] - ti;
        re[a] += tr;
        im[a] += ti;
      }
    }
  }

  // One-sided band power, normalized so the bands add up to the frame's mean square (Parseval)
  static constexpr float BAND_SCALE = 2.0f / (static_cast<float>(FFT_SIZE) * WINDOW_POWER_SUM);
  float band_db[NUM_BANDS];
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    float sum = 0.0f;
    for (uint16_t k = this->band_bins_[b]; k < this->band_bins_[b + 1]; ++k) {
      sum += re[k] * re[k] + im[k] * im[k];
    }
    band_db[b] = to_db(sum * BAND_SCALE);
  }

  this->apply_targets_(to_db(squared_sum / FFT_SIZE), band_db, this->get_frame_seconds());
}

void SpectrumAnalysis::apply_targets_(float rms_db, const float *band_db, float seconds) {
  const float attack = smoothing_alpha(seconds, this->attack_ms_);
  const float release = smoothing_alpha(seconds, this->release_ms_);

  this->rms_db_ += (rms_db - this->rms_db_) * (rms_db > this->rms_db_ ? attack : release);
  for (uint8_t b = 0; b < NUM_BANDS; ++b) {
    float &db = this->band_db_[b];
    db += (band_db[b] - db) * (band_db[b] > db ? attack : release);
  }
}

void SpectrumAnalysis::release(float seconds) {
  if (this->is_released()) {
    return;
  }
  float silence[NUM_BANDS];
  std::fill(silence, silence + NUM_BANDS, FLOOR_DB);
  this->apply_targets_(FLOOR_DB, silence, seconds);
}

bool SpectrumAnalysis::is_released() const {
  static constexpr float RELEASED_DB = FLOOR_DB + 0.1f;
  if (this->rms_db_ > RELEASED_DB) {
    return false;
  }
  return std::all_of(this->band_db_.begin(), this->band_db_.end(), [](float db) { return db <= RELEASED_DB; });
}

}  // namespace esphome::audio_analyzer
