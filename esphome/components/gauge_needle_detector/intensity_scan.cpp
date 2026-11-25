#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include <algorithm>

namespace esphome::gauge_needle_detector {

#ifdef USE_GAUGE_NEEDLE_INTENSITY_SCAN

namespace {

constexpr int MAX_GAP = 5;
constexpr int DARK_BACKGROUND_MIN = 150;
constexpr int LIGHT_BACKGROUND_MAX = 100;

DetectionResult scan_intensity(const uint8_t *raw, const ScanWindow &window, const DetectorConfig &config,
                               float *scores) {
  if (raw == nullptr || !window.valid())
    return {};

  for (int deg = 0; deg < ANGLE_COUNT; deg++) {
    const float dx = cos_lut(deg);
    const float dy = sin_lut(deg);

    float score = 0.0f;
    int gap_count = 0;

    for (int r = window.start_r; r < window.end_r; r++) {
      const int px = window.cx + static_cast<int>(r * dx);
      const int py = window.cy + static_cast<int>(r * dy);

      if (px >= 0 && px < window.width && py >= 0 && py < window.height) {
        const uint8_t val = raw[py * window.width + px];

        score += (config.needle_type == NEEDLE_TYPE_LIGHT) ? val : (255 - val);

        const bool background =
            (config.needle_type == NEEDLE_TYPE_DARK) ? val > DARK_BACKGROUND_MIN : val < LIGHT_BACKGROUND_MAX;
        gap_count = background ? gap_count + 1 : 0;

        if (gap_count > MAX_GAP)
          break;
      }
    }
    scores[deg] = score;
  }

  float best_score = -1.0f;
  float best_angle = 0;
  float total = 0.0f;
  for (int deg = 0; deg < ANGLE_COUNT; deg++) {
    if (scores[deg] > best_score) {
      best_score = scores[deg];
      best_angle = static_cast<float>(deg);
    }
    total += scores[deg];
  }

  // Support is how far the best ray rises above the average ray, relative to the
  // crop's headroom, so a bright or busy background does not count as needle.
  const float sampled = static_cast<float>(window.end_r - window.start_r);
  const float mean = total / (static_cast<float>(ANGLE_COUNT) * sampled);
  const float headroom = 255.0f - mean;
  const float confidence =
      headroom > 0.0f ? std::clamp((best_score - mean * sampled) / (sampled * headroom), 0.0f, 1.0f) : 0.0f;

  return {best_angle, confidence};
}

}  // namespace

DetectionResult IntensityScanDetector::detect(const uint8_t *raw, const uint8_t * /*filtered*/,
                                              const ScanWindow &window, const DetectorConfig &config, float *scores) {
  return scan_intensity(raw, window, config, scores);
}

#endif  // USE_GAUGE_NEEDLE_INTENSITY_SCAN

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
