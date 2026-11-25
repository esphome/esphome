#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include <algorithm>
#include <cmath>

namespace esphome::gauge_needle_detector {

#ifdef USE_GAUGE_NEEDLE_RADIAL_PROFILE

namespace {

constexpr int MAX_GAP = 5;
constexpr int SIGNAL_THRESHOLD = 20;
constexpr float PARABOLA_EPSILON = 0.0001f;

DetectionResult scan_radial_profile(const uint8_t *filtered, const ScanWindow &window, float *scores) {
  if (filtered == nullptr || !window.valid())
    return {};

  for (int deg = 0; deg < ANGLE_COUNT; deg++) {
    const float dx = cos_lut(deg);
    const float dy = sin_lut(deg);

    float score = 0.0f;
    int gap_count = 0;

    // Stop at the first large gap to skip the short tail of the needle.
    for (int r = window.start_r; r < window.end_r; r++) {
      const int px = window.cx + static_cast<int>(r * dx);
      const int py = window.cy + static_cast<int>(r * dy);

      if (px < 0 || px >= window.width || py < 0 || py >= window.height)
        break;

      const uint8_t val = filtered[py * window.width + px];
      if (val > SIGNAL_THRESHOLD) {
        score += val;
        gap_count = 0;
      } else if (++gap_count > MAX_GAP) {
        break;
      }
    }

    scores[deg] = score;
  }

  // The top-hat image always has a bright needle, so the highest score wins.
  const ScoreRange range = find_score_range(scores);
  const int best_angle_int = range.best_angle;

  // Parabolic interpolation around the peak for sub-degree precision.
  float best_angle = static_cast<float>(best_angle_int);
  const int prev = (best_angle_int - 1 + ANGLE_COUNT) % ANGLE_COUNT;
  const int next = (best_angle_int + 1) % ANGLE_COUNT;
  const float y1 = scores[prev];
  const float y2 = scores[best_angle_int];
  const float y3 = scores[next];
  const float denom = y1 - 2 * y2 + y3;
  if (std::fabs(denom) > PARABOLA_EPSILON)
    best_angle += (y1 - y3) / (2 * denom);

  if (best_angle < 0)
    best_angle += 360.0f;
  if (best_angle >= static_cast<float>(ANGLE_COUNT))
    best_angle -= static_cast<float>(ANGLE_COUNT);

  // Support is the covered share of the ray; contrast says the best ray stands out.
  const float sampled = static_cast<float>(window.end_r - window.start_r);
  const float confidence = ray_confidence(range.best / (sampled * 255.0f), range.best, range.worst);

  return {best_angle, confidence};
}

}  // namespace

DetectionResult RadialProfileDetector::detect(const uint8_t * /*raw*/, const uint8_t *filtered,
                                              const ScanWindow &window, const DetectorConfig & /*config*/,
                                              float *scores) {
  return scan_radial_profile(filtered, window, scores);
}

#endif  // USE_GAUGE_NEEDLE_RADIAL_PROFILE

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
