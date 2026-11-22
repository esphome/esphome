#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include "esphome/core/log.h"

#include <algorithm>
#include <cfloat>

namespace esphome::gauge_needle_detector {

#ifdef USE_GAUGE_NEEDLE_TEMPLATE_MATCH

ESPHOME_LOG_TAG(TAG, "gauge_needle_detector");

DetectionResult TemplateMatchDetector::detect(const uint8_t * /*raw*/, const uint8_t *filtered,
                                              const ScanWindow &window, const DetectorConfig &config, float *scores) {
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
  float best_score = -FLT_MAX;
  float min_score = FLT_MAX;
  int best_angle_int = 0;
  for (int deg = 0; deg < ANGLE_COUNT; deg++) {
    const float score = ray_score(deg);
    scores[deg] = score;
    if (score > best_score) {
      best_score = score;
      best_angle_int = deg;
    }
    min_score = std::min(min_score, score);
  }

  // Same scale as radial_profile, so `auto` can rank the two.
  const float confidence = ray_confidence(std::max(0.0f, best_score) / 255.0f, best_score, min_score);
  if (config.debug) {
    ESP_LOGD(TAG, "Template Match: Angle=%d, Conf=%.2f", best_angle_int, confidence);
  }

  return {static_cast<float>(best_angle_int), confidence};
}

#endif  // USE_GAUGE_NEEDLE_TEMPLATE_MATCH

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
