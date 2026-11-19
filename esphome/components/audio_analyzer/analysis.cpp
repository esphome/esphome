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

float read_mono_sample(const uint8_t *frame, uint8_t bytes_per_sample, uint8_t channels) {
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
  return channels > 1 ? sum / channels : sum;
}

void Decimator::configure(uint32_t sample_rate) {
  this->input_rate_ = sample_rate;
  this->factor_ = decimation_for(sample_rate);
  this->inv_factor_ = 1.0f / this->factor_;
  this->output_rate_ = static_cast<float>(sample_rate) / this->factor_;
  this->sum_ = 0.0f;
  this->count_ = 0;

  // 4th-order Butterworth low-pass as two biquads (Q of each pole pair), corner at MAX_BAND_FRACTION of the output
  // rate (RBJ cookbook coefficients)
  static constexpr double SECTION_Q[2] = {0.54119610014619698, 1.3065629648763766};
  const double w0 = 2.0 * PI * MAX_BAND_FRACTION / this->factor_;
  const double cos_w0 = std::cos(w0);
  for (uint8_t s = 0; s < 2; ++s) {
    const double alpha = std::sin(w0) / (2.0 * SECTION_Q[s]);
    const double a0 = 1.0 + alpha;
    Biquad &section = this->sections_[s];
    section.b0 = static_cast<float>((1.0 - cos_w0) / 2.0 / a0);
    section.b1 = static_cast<float>((1.0 - cos_w0) / a0);
    section.b2 = section.b0;
    section.a1 = static_cast<float>(-2.0 * cos_w0 / a0);
    section.a2 = static_cast<float>((1.0 - alpha) / a0);
    section.z1 = 0.0f;
    section.z2 = 0.0f;
  }
}

bool SpectrumAnalysis::configure(float analysis_rate) {
  this->analysis_rate_ = analysis_rate;
  this->frame_fill_ = 0;

  const float limit = MAX_BAND_FRACTION * analysis_rate;
  const bool clamped = this->max_frequency_ > limit;
  this->upper_frequency_ = clamped ? limit : this->max_frequency_;

  const float bin_width = analysis_rate / FFT_SIZE;
  const float ratio = this->upper_frequency_ / this->min_frequency_;
  for (uint8_t b = 0; b <= NUM_BANDS; ++b) {
    const float edge_hz = this->min_frequency_ * powf(ratio, static_cast<float>(b) / NUM_BANDS);
    uint16_t bin =
        static_cast<uint16_t>(std::clamp<int32_t>(static_cast<int32_t>(lroundf(edge_hz / bin_width)), 1, FFT_SIZE / 2));
    if (b > 0 && bin <= this->band_bins_[b - 1]) {
      // Config validation keeps bands at least one bin wide at the highest analysis rate; a Nyquist clamp can
      // still narrow them, so every band keeps at least one bin
      bin = this->band_bins_[b - 1] + 1;
    }
    this->band_bins_[b] = bin;
  }
  return !clamped;
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

void SpectrumAnalysis::apply_targets_(float power_db, const float *band_db, float seconds) {
  const float attack = smoothing_alpha(seconds, this->attack_ms_);
  const float release = smoothing_alpha(seconds, this->release_ms_);

  this->power_db_ += (power_db - this->power_db_) * (power_db > this->power_db_ ? attack : release);
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
  if (this->power_db_ > RELEASED_DB) {
    return false;
  }
  return std::all_of(this->band_db_.begin(), this->band_db_.end(), [](float db) { return db <= RELEASED_DB; });
}

}  // namespace esphome::audio_analyzer
