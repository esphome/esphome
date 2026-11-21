#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include <algorithm>

namespace esphome::gauge_needle_detector {

#ifdef USE_GAUGE_NEEDLE_INTENSITY_SCAN

namespace {

/// Intensity along each ray, stopping at the first long background run.
DetectionResult scan_intensity(const uint8_t *raw, int width, int height, const DetectorConfig &config, float *scores,
                               int max_gap = 5, int dark_background_min = 150, int light_background_max = 100) {
  if (raw == nullptr)
    return {};

  const int cx = width / 2;
  const int cy = height / 2;
  const int radius = std::min(cx, cy) - 2;
  if (radius <= 0)
    return {};

  const int deadzone_r = static_cast<int>(std::max(0.0f, config.deadzone_diameter * 0.5f));
  const int start_r = std::max({1, static_cast<int>(radius * config.min_scan_radius), deadzone_r});
  const int end_r = static_cast<int>(radius * config.max_scan_radius);
  if (start_r >= end_r)
    return {};

  for (int deg = 0; deg < 360; deg++) {
    const float dx = cos_lut(deg);
    const float dy = sin_lut(deg);

    float score = 0.0f;
    int gap_count = 0;

    for (int r = start_r; r < end_r; r++) {
      const int px = cx + static_cast<int>(r * dx);
      const int py = cy + static_cast<int>(r * dy);

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
  float min_score = scores[0];
  float max_score = scores[0];
  for (int deg = 0; deg < 360; deg++) {
    if (scores[deg] > best_score) {
      best_score = scores[deg];
      best_angle = static_cast<float>(deg);
    }
    min_score = std::min(min_score, scores[deg]);
    max_score = std::max(max_score, scores[deg]);
  }

  // Equal scores mean there is no direction to report, including a blank crop.
  const float confidence = (max_score > min_score && max_score > 0.0f) ? ((max_score - min_score) / max_score) : 0.0f;

  return {best_angle, confidence};
}

}  // namespace

DetectionResult IntensityScanDetector::detect(const uint8_t *raw, const uint8_t * /*filtered*/, int width, int height,
                                              const DetectorConfig &config, float *scores) {
  return scan_intensity(raw, width, height, config, scores);
}

#endif  // USE_GAUGE_NEEDLE_INTENSITY_SCAN

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
