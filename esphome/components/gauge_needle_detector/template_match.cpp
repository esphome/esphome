#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include <algorithm>

namespace esphome::gauge_needle_detector {

#ifdef USE_GAUGE_NEEDLE_TEMPLATE_MATCH

DetectionResult TemplateMatchDetector::detect(const uint8_t * /*raw*/, const uint8_t *filtered,
                                              const ScanWindow &window, const DetectorConfig & /*config*/,
                                              float *scores) {
  if (filtered == nullptr || !window.valid())
    return {};

  auto ray_score = [&](int degrees) -> float {
    const float dx = cos_lut(degrees);
    const float dy = sin_lut(degrees);
    float score = 0.0f;
    int count = 0;
    for (int r = window.start_r; r < window.end_r; r++) {
      const int px = window.cx + static_cast<int>(r * dx);
      const int py = window.cy + static_cast<int>(r * dy);
      if (px >= 0 && px < window.width && py >= 0 && py < window.height) {
        score += filtered[py * window.width + px];
        count++;
      }
    }
    return count > 0 ? score / count : -1.0f;
  };

  // Every whole degree, because a one-pixel needle can fall between coarser rays.
  for (int deg = 0; deg < ANGLE_COUNT; deg++)
    scores[deg] = ray_score(deg);
  const ScoreRange range = find_score_range(scores);

  // Same scale as radial_profile, so `auto` can rank the two.
  const float confidence = ray_confidence(std::max(0.0f, range.best) / 255.0f, range.best, range.worst);

  return {static_cast<float>(range.best_angle), confidence};
}

#endif  // USE_GAUGE_NEEDLE_TEMPLATE_MATCH

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
