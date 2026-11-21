#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include "esphome/core/log.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace esphome::gauge_needle_detector {

#ifdef USE_GAUGE_NEEDLE_RADIAL_PROFILE

ESPHOME_LOG_TAG(TAG, "gauge_needle_detector");

namespace {

/// Traces rays from the centre and keeps the longest bright run.
DetectionResult scan_radial_profile(const uint8_t *filtered, int width, int height, const DetectorConfig &config,
                                    float *scores, int max_gap = 5, int signal_threshold = 20,
                                    float parabola_epsilon = 0.0001f) {
  if (filtered == nullptr)
    return {};

  const int cx = width / 2;
  const int cy = height / 2;
  const int radius = std::min(cx, cy) - 2;
  if (radius <= 0)
    return {};

  const int deadzone_r = static_cast<int>(std::max(0.0f, config.deadzone_diameter * 0.5f));
  const int start_r = std::max({1, static_cast<int>(radius * config.min_scan_radius), deadzone_r});
  const int max_radius_scan = static_cast<int>(radius * config.max_scan_radius);
  if (start_r >= max_radius_scan) {
    ESP_LOGW(TAG, "Deadzone (%dpx radius) exceeds scan radius (%dpx); no pixels can be sampled", deadzone_r,
             max_radius_scan);
    return {};
  }

  for (int deg = 0; deg < 360; deg++) {
    const float dx = cos_lut(deg);
    const float dy = sin_lut(deg);

    float score = 0.0f;
    int gap_count = 0;

    // Stop at the first large gap to skip the short tail of the needle.
    for (int r = start_r; r < max_radius_scan; r++) {
      const int px = cx + static_cast<int>(r * dx);
      const int py = cy + static_cast<int>(r * dy);

      if (px < 0 || px >= width || py < 0 || py >= height)
        break;

      const uint8_t val = filtered[py * width + px];
      if (val > signal_threshold) {
        score += val;
        gap_count = 0;
      } else if (++gap_count > max_gap) {
        break;
      }
    }

    scores[deg] = score;
  }

  // The top-hat image always has a bright needle, so the highest score wins.
  float best_score = -FLT_MAX;
  float min_score = FLT_MAX;
  int best_angle_int = 0;
  for (int deg = 0; deg < 360; deg++) {
    if (scores[deg] > best_score) {
      best_score = scores[deg];
      best_angle_int = deg;
    }
    min_score = std::min(min_score, scores[deg]);
  }

  // Parabolic interpolation around the peak for sub-degree precision.
  float best_angle = static_cast<float>(best_angle_int);
  const int prev = (best_angle_int - 1 + 360) % 360;
  const int next = (best_angle_int + 1) % 360;
  const float y1 = scores[prev];
  const float y2 = scores[best_angle_int];
  const float y3 = scores[next];
  const float denom = y1 - 2 * y2 + y3;
  if (std::fabs(denom) > parabola_epsilon)
    best_angle += (y1 - y3) / (2 * denom);

  if (best_angle < 0)
    best_angle += 360.0f;
  if (best_angle >= 360.0f)
    best_angle -= 360.0f;

  // Support is the share of the sampled ray the needle covers; contrast says
  // the best ray stands out from the darkest one.
  const float sampled = static_cast<float>(max_radius_scan - start_r);
  const float support = sampled > 0.0f ? best_score / (sampled * 255.0f) : 0.0f;
  const float contrast = best_score > 0.0f ? (best_score - min_score) / best_score : 0.0f;
  const float confidence = std::clamp(support * (0.5f + 0.5f * contrast), 0.0f, 1.0f);

  if (config.debug) {
    ESP_LOGD(TAG, "Radial Profile: Best Angle=%.1f, Conf=%.2f", best_angle, confidence);
  }

  return {best_angle, confidence};
}

}  // namespace

DetectionResult RadialProfileDetector::detect(const uint8_t * /*raw*/, const uint8_t *filtered, int width, int height,
                                              const DetectorConfig &config, float *scores) {
  return scan_radial_profile(filtered, width, height, config, scores);
}

#endif  // USE_GAUGE_NEEDLE_RADIAL_PROFILE

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
