#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace esphome::i2s_aec_microphone {

// A diagnostic projection of the raw and processed microphone signals onto
// nearby speaker-reference delays. This is not an ERLE estimator: late
// reflections and near-end sounds can remain outside the selected projection.
class ReferenceProbe {
 public:
  static constexpr std::array<int, 7> LAG_OFFSETS{-8, -5, -1, 0, 1, 5, 8};
  static constexpr int LAG_COUNT = LAG_OFFSETS.size();
  static constexpr int MAX_LAG_OFFSET = 8;
  // Keep the observer's PSRAM reads small enough not to disturb the AEC task.
  // Rotate the phase between frames so all sample positions are represented.
  static constexpr int SAMPLE_STRIDE = 128;
  static constexpr int WINDOW_FRAMES = 128;

  struct Result {
    bool valid{false};
    int lag_index{LAG_COUNT / 2};
    float remaining{0.0f};
    float raw_correlation_squared{0.0f};
    float reference_rms{0.0f};
  };

  void reset() { this->lags_ = {}; }

  void add_before(int lag_index, int16_t reference, int16_t raw) {
    auto &lag = this->lags_[lag_index];
    lag.reference_sum += reference;
    lag.raw_sum += raw;
    lag.reference_energy += static_cast<int64_t>(reference) * reference;
    lag.raw_energy += static_cast<int64_t>(raw) * raw;
    lag.raw_cross += static_cast<int64_t>(reference) * raw;
    ++lag.samples;
  }

  void add_after(int lag_index, int16_t reference, int16_t clean) {
    auto &lag = this->lags_[lag_index];
    lag.clean_sum += clean;
    lag.clean_cross += static_cast<int64_t>(reference) * clean;
  }

  Result result() const {
    Result result;
    double best_power = -1.0;
    for (int index = 0; index < LAG_COUNT; ++index) {
      const auto &lag = this->lags_[index];
      if (lag.samples == 0)
        continue;
      const double count = lag.samples;
      const double reference_energy = static_cast<double>(lag.reference_energy) -
                                      static_cast<double>(lag.reference_sum) * lag.reference_sum / count;
      const double raw_energy =
          static_cast<double>(lag.raw_energy) - static_cast<double>(lag.raw_sum) * lag.raw_sum / count;
      if (reference_energy <= 0.0 || raw_energy <= 0.0)
        continue;
      const double raw_cross =
          static_cast<double>(lag.raw_cross) - static_cast<double>(lag.reference_sum) * lag.raw_sum / count;
      const double power = raw_cross * raw_cross / reference_energy;
      if (power < best_power || (power == best_power && result.valid &&
                                 std::abs(LAG_OFFSETS[index]) >= std::abs(LAG_OFFSETS[result.lag_index])))
        continue;
      best_power = power;
      result.valid = true;
      result.lag_index = index;
      result.reference_rms = static_cast<float>(std::sqrt(reference_energy / count));
      result.raw_correlation_squared = static_cast<float>(power / raw_energy);
      const double clean_cross =
          static_cast<double>(lag.clean_cross) - static_cast<double>(lag.reference_sum) * lag.clean_sum / count;
      // Both projections use the same raw-microphone normalization. The small,
      // dimensionless floor avoids an unstable ratio when the raw projection
      // approaches zero; it is not a measured noise level or an ERLE estimate.
      static constexpr double CORRELATION_FLOOR = 0.001;
      const double denominator = std::sqrt(reference_energy * raw_energy);
      const double before = raw_cross / denominator;
      const double after = clean_cross / denominator;
      result.remaining = static_cast<float>(std::sqrt((after * after + CORRELATION_FLOOR * CORRELATION_FLOOR) /
                                                      (before * before + CORRELATION_FLOOR * CORRELATION_FLOOR)));
    }
    return result;
  }

 private:
  struct LagSums {
    int64_t reference_sum{0};
    int64_t raw_sum{0};
    int64_t clean_sum{0};
    int64_t reference_energy{0};
    int64_t raw_energy{0};
    int64_t raw_cross{0};
    int64_t clean_cross{0};
    uint32_t samples{0};
  };

  std::array<LagSums, LAG_COUNT> lags_{};
};

}  // namespace esphome::i2s_aec_microphone
