#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include "esphome/components/audio_analyzer/analysis.h"

namespace esphome::audio_analyzer::testing {

static constexpr double TWO_PI = 6.283185307179586;

/// The band plan for the defaults (100-4000 Hz) at 16 kHz.
static constexpr std::array<uint16_t, NUM_BANDS + 1> DEFAULT_PLAN{3, 5, 7, 9, 11, 14, 20, 27, 37, 50, 68, 94, 128};
/// The plan for min_frequency 20 Hz at 16 kHz: the first band starts at bin 2, above the bins DC reaches.
static constexpr std::array<uint16_t, NUM_BANDS + 1> LOW_PLAN{2, 4, 6, 8, 10, 12, 16, 23, 32, 45, 64, 91, 128};

inline float mean_square_db(float amplitude) { return 10.0f * log10f(amplitude * amplitude / 2.0f); }

/// The 8th-order Butterworth anti-alias filter, corner at 0.4 of the decimated rate, designed in double.
inline std::array<Decimator::Section, Decimator::NUM_SECTIONS> reference_sections(uint8_t factor) {
  std::array<Decimator::Section, Decimator::NUM_SECTIONS> sections{};
  const double w0 = TWO_PI * 0.4 / factor;
  for (int k = 0; k < Decimator::NUM_SECTIONS; ++k) {
    const double q = 1.0 / (2.0 * std::cos(TWO_PI / 2 * (2 * k + 1) / 16.0));
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;
    sections[k] = {static_cast<float>((1.0 - std::cos(w0)) / 2.0 / a0), static_cast<float>(-2.0 * std::cos(w0) / a0),
                   static_cast<float>((1.0 - alpha) / a0)};
  }
  return sections;
}

}  // namespace esphome::audio_analyzer::testing
