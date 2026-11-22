#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::gauge_needle_detector {

#ifdef USE_GAUGE_NEEDLE_HOUGH_TRANSFORM

ESPHOME_LOG_TAG(TAG, "gauge_needle_detector");

static constexpr int NUM_ANGLES = 360;

namespace {

/// Votes edge pixels into a radial histogram, weighted by edge magnitude.
DetectionResult detect_hough(const uint8_t *filtered, int width, int height, const DetectorConfig &config,
                             std::array<int, NUM_ANGLES> &accumulator, int vote_magnitude_min = 40) {
  if (filtered == nullptr)
    return {};

  const int cx = width / 2;
  const int cy = height / 2;
  const int radius = std::min(cx, cy) - 2;
  // A tiny crop has no radius, and the confidence below divides by the scan length.
  if (radius <= 0)
    return {};

  const int deadzone_r = static_cast<int>(std::max(0.0f, config.deadzone_diameter * 0.5f));
  const int min_r = std::max({1, static_cast<int>(radius * config.min_scan_radius), deadzone_r});
  const int max_r = static_cast<int>(radius * config.max_scan_radius);
  if (min_r >= max_r)
    return {};

  std::fill(accumulator.begin(), accumulator.end(), 0);
  int total_votes = 0;

  // Sobel magnitude and the vote from that pixel happen in one pass.
  for (int y = 1; y < height - 1; y++) {
    for (int x = 1; x < width - 1; x++) {
      const int gx = -filtered[(y - 1) * width + (x - 1)] - 2 * filtered[y * width + (x - 1)] -
                     filtered[(y + 1) * width + (x - 1)] + filtered[(y - 1) * width + (x + 1)] +
                     2 * filtered[y * width + (x + 1)] + filtered[(y + 1) * width + (x + 1)];

      const int gy = -filtered[(y - 1) * width + (x - 1)] - 2 * filtered[(y - 1) * width + x] -
                     filtered[(y - 1) * width + (x + 1)] + filtered[(y + 1) * width + (x - 1)] +
                     2 * filtered[(y + 1) * width + x] + filtered[(y + 1) * width + (x + 1)];

      const int magnitude = static_cast<int>(std::sqrt(static_cast<float>(gx * gx + gy * gy)));
      if (magnitude <= vote_magnitude_min)
        continue;

      const float dx = x - cx;
      const float dy = y - cy;
      const float r = std::sqrt(dx * dx + dy * dy);
      if (r < min_r || r > max_r)
        continue;

      float theta = radians_to_degrees(std::atan2(dy, dx));
      if (theta < 0)
        theta += 360.0f;

      const int vote = std::min(magnitude, 255);
      accumulator[static_cast<int>(theta) % NUM_ANGLES] += vote;
      total_votes += vote;
    }
  }

  int max_votes = 0;
  int best_angle_idx = 0;
  for (int i = 0; i < NUM_ANGLES; i++) {
    if (accumulator[i] > max_votes) {
      max_votes = accumulator[i];
      best_angle_idx = i;
    }
  }

  // Support is how much of one ray voted and separation says the votes
  // concentrate; two edges per radius step are budgeted so it cannot saturate.
  const float support = static_cast<float>(max_votes) / (2.0f * (max_r - min_r + 1) * 255.0f);
  const float mean = static_cast<float>(total_votes) / NUM_ANGLES;
  const float separation = max_votes > 0 ? (max_votes - mean) / max_votes : 0.0f;
  const float confidence = std::clamp(support * separation, 0.0f, 1.0f);

  if (config.debug) {
    ESP_LOGD(TAG, "Hough Transform: Best Angle Index=%d, Conf=%.2f", best_angle_idx, confidence);
  }

  return {static_cast<float>(best_angle_idx), confidence};
}

}  // namespace

DetectionResult HoughTransformDetector::detect(const uint8_t * /*raw*/, const uint8_t *filtered, int width, int height,
                                               const DetectorConfig &config, float * /*scores*/) {
  return detect_hough(filtered, width, height, config, this->accumulator_);
}

#endif  // USE_GAUGE_NEEDLE_HOUGH_TRANSFORM

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
