#include "color_helpers.h"

#include <algorithm>
#include <cmath>

namespace esphome::as734x {

namespace {

// Range McCamy's approximation is meaningful over.
constexpr float CCT_MIN_K = 1000.0f;
constexpr float CCT_MAX_K = 25000.0f;

#ifdef USE_AS734X_RGB
// sRGB transfer function.
float gamma_correct(float channel) {
  if (channel <= 0.0031308f) {
    return 12.92f * channel;
  }
  return 1.055f * std::pow(channel, 1.0f / 2.4f) - 0.055f;
}

uint8_t to_byte(float channel) { return static_cast<uint8_t>(std::clamp(channel, 0.0f, 1.0f) * 255.0f); }
#endif

}  // namespace

float tristimulus_to_cct(float x, float y, float z) {
  const float sum = x + y + z;
  // The conversion matrices carry negative coefficients, so a spectrum unlike the one they were
  // calibrated against can drive the sum to zero or below. Chromaticity is undefined there, and
  // McCamy's formula would still return a plausible looking number from it.
  if (sum <= MIN_TRISTIMULUS_SUM) {
    return NAN;
  }
  const float chroma_x = x / sum;
  const float chroma_y = y / sum;

  // McCamy's approximation: cct = 437 n^3 + 3601 n^2 + 6861 n + 5517. The pole at
  // chroma_y = 0.1858 sends the cubic off to either infinity, and it stays meaningless well
  // either side of that, so the answer is only reported where the approximation holds.
  const float denominator = 0.1858f - chroma_y;
  if (std::abs(denominator) < 1e-4f) {
    return NAN;
  }
  const float n = (chroma_x - 0.3320f) / denominator;
  const float cct = ((437.0f * n + 3601.0f) * n + 6861.0f) * n + 5517.0f;
  return (cct >= CCT_MIN_K && cct <= CCT_MAX_K) ? cct : NAN;
}

#ifdef USE_AS734X_RGB
void tristimulus_to_rgb(float x, float y, float z, uint8_t &r, uint8_t &g, uint8_t &b) {
  const float r_linear = 3.2406f * x - 1.5372f * y - 0.4986f * z;
  const float g_linear = -0.9689f * x + 1.8758f * y + 0.0415f * z;
  const float b_linear = 0.0557f * x - 0.2040f * y + 1.0570f * z;

  r = to_byte(gamma_correct(r_linear));
  g = to_byte(gamma_correct(g_linear));
  b = to_byte(gamma_correct(b_linear));
}
#endif

}  // namespace esphome::as734x
