#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "esphome/components/audio_analyzer/analysis.h"
#include "esphome/core/application.h"

namespace esphome::audio_analyzer::testing {

static constexpr double TWO_PI = 6.283185307179586;

/// The band plan code generation makes for the defaults (100-4000 Hz) at 16 kHz; test_config.py checks it.
static constexpr std::array<uint16_t, NUM_BANDS + 1> DEFAULT_PLAN{3, 5, 7, 9, 11, 14, 20, 27, 37, 50, 68, 94, 128};

inline float mean_square_db(float amplitude) { return 10.0f * log10f(amplitude * amplitude / 2.0f); }

inline void at(uint32_t now_ms) { LoopBlockingGuard clock{nullptr, nullptr, now_ms}; }

/// The anti-alias filter code generation designs (__init__.py anti_alias_sections), in double: a reference for the
/// tests, whose design is checked against its specification in test_config.py.
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

/// 16-bit little-endian PCM of a sine, ``channels`` identical channels.
inline std::vector<uint8_t> pcm_sine(uint32_t frames, double frequency, uint32_t rate, float amplitude,
                                     uint8_t channels = 1, uint32_t first_frame = 0) {
  std::vector<uint8_t> bytes;
  bytes.reserve(frames * channels * 2);
  for (uint32_t i = 0; i < frames; ++i) {
    const auto value = static_cast<int16_t>(
        std::lround(32767.0 * amplitude * std::sin(TWO_PI * frequency * (first_frame + i) / rate)));
    for (uint8_t c = 0; c < channels; ++c) {
      bytes.push_back(static_cast<uint8_t>(value & 0xFF));
      bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }
  }
  return bytes;
}

}  // namespace esphome::audio_analyzer::testing
