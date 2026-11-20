#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include <algorithm>
#include <cmath>

namespace esphome::gauge_needle_detector {

#ifdef USE_GAUGE_NEEDLE_INTENSITY_SCAN

namespace {

/// Intensity along each ray, stopping at the first long background run.
DetectionResult scan_intensity(const uint8_t *raw, int width, int height, const DetectorConfig &config,
                               std::array<float, 360> &scores, float scan_start_radius = 0.15f,
                               float scan_end_radius = 0.9f, int max_gap = 5, int dark_background_min = 150,
                               int light_background_max = 100) {
  if (raw == nullptr)
    return {};

  const int cx = width / 2;
  const int cy = height / 2;
  const int radius = std::min(cx, cy) - 2;

  const int start_r = static_cast<int>(radius * scan_start_radius);
  const int end_r = static_cast<int>(radius * scan_end_radius);

  for (int deg = 0; deg < 360; deg++) {
    const float rad = degrees_to_radians(static_cast<float>(deg));
    const float dx = std::cos(rad);
    const float dy = std::sin(rad);

    float score = 0.0f;
    int gap_count = 0;

    for (int r = start_r; r < end_r; r++) {
      const int px = cx + static_cast<int>(std::lround(r * dx));
      const int py = cy + static_cast<int>(std::lround(r * dy));

      if (px >= 0 && px < width && py >= 0 && py < height) {
        const uint8_t val = raw[py * width + px];

        // A light needle scores on brightness, a dark needle on darkness.
        score += (config.needle_type == NEEDLE_TYPE_LIGHT) ? val : (255 - val);

        const bool background =
            (config.needle_type == NEEDLE_TYPE_DARK) ? val > dark_background_min : val < light_background_max;
        gap_count = background ? gap_count + 1 : 0;

        if (gap_count > max_gap)
          break;
      }
    }
    scores[deg] = score;
  }

  float best_score = -1.0f;
  float best_angle = 0;
  for (int deg = 0; deg < 360; deg++) {
    if (scores[deg] > best_score) {
      best_score = scores[deg];
      best_angle = static_cast<float>(deg);
    }
  }

  const float min_score = *std::min_element(scores.begin(), scores.end());
  const float max_score = *std::max_element(scores.begin(), scores.end());
  const float confidence = (max_score > min_score) ? ((max_score - min_score) / max_score) : 0.5f;

  return {best_angle, confidence};
}

}  // namespace

DetectionResult IntensityScanDetector::detect(const uint8_t *raw, const uint8_t * /*filtered*/, int width, int height,
                                              const DetectorConfig &config) {
  return scan_intensity(raw, width, height, config, this->scores_);
}

#endif  // USE_GAUGE_NEEDLE_INTENSITY_SCAN

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
