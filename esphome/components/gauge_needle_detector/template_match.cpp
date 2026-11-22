#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include "esphome/core/log.h"

#include <algorithm>
#include <cfloat>

namespace esphome::gauge_needle_detector {

#ifdef USE_GAUGE_NEEDLE_TEMPLATE_MATCH

ESPHOME_LOG_TAG(TAG, "gauge_needle_detector");

DetectionResult TemplateMatchDetector::detect(const uint8_t * /*raw*/, const uint8_t *filtered, int width, int height,
                                              const DetectorConfig &config, float *scores) {
  if (filtered == nullptr)
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

  // Average brightness along one ray.
  auto ray_score = [&](int degrees) -> float {
    const float dx = cos_lut(degrees);
    const float dy = sin_lut(degrees);
    float score = 0.0f;
    int count = 0;
    for (int r = start_r; r < end_r; r++) {
      const int px = cx + static_cast<int>(r * dx);
      const int py = cy + static_cast<int>(r * dy);
      if (px >= 0 && px < width && py >= 0 && py < height) {
        score += filtered[py * width + px];
        count++;
      }
    }
    return count > 0 ? score / count : -1.0f;
  };

  // Every whole degree, because a one-pixel needle can fall between coarser rays.
  float best_score = -FLT_MAX;
  float min_score = FLT_MAX;
  int best_angle_int = 0;
  for (int deg = 0; deg < 360; deg++) {
    const float score = ray_score(deg);
    scores[deg] = score;
    if (score > best_score) {
      best_score = score;
      best_angle_int = deg;
    }
    min_score = std::min(min_score, score);
  }

  // Same scale as radial_profile: support is how bright the best ray is and
  // contrast says it stands out, so `auto` can rank the two against each other.
  const float support = std::max(0.0f, best_score) / 255.0f;
  const float contrast = best_score > 0.0f ? (best_score - min_score) / best_score : 0.0f;
  const float confidence = std::clamp(support * (0.5f + 0.5f * contrast), 0.0f, 1.0f);
  if (config.debug) {
    ESP_LOGD(TAG, "Template Match: Angle=%d, Conf=%.2f", best_angle_int, confidence);
  }

  return {static_cast<float>(best_angle_int), confidence};
}

#endif  // USE_GAUGE_NEEDLE_TEMPLATE_MATCH

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
