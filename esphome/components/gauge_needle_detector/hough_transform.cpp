#include "gauge_needle_detector.h"

#ifdef USE_GAUGE_NEEDLE_DETECTOR

#include <algorithm>
#include <cmath>

namespace esphome::gauge_needle_detector {

#ifdef USE_GAUGE_NEEDLE_HOUGH_TRANSFORM

namespace {

/// Votes edge pixels into a radial histogram, weighted by edge magnitude.
constexpr int VOTE_MAGNITUDE_MIN = 40;

DetectionResult detect_hough(const uint8_t *filtered, const ScanWindow &window, float *scores) {
  if (filtered == nullptr || !window.valid())
    return {};

  std::fill(scores, scores + ANGLE_COUNT, 0.0f);
  float total_votes = 0.0f;

  // Only the scan annulus is inspected, so no Sobel work is spent outside it.
  const int min_r_sq = window.start_r * window.start_r;
  const int max_r_sq = window.end_r * window.end_r;
  const int x0 = std::max(1, window.cx - window.end_r);
  const int x1 = std::min(window.width - 2, window.cx + window.end_r);
  const int y0 = std::max(1, window.cy - window.end_r);
  const int y1 = std::min(window.height - 2, window.cy + window.end_r);

  for (int y = y0; y <= y1; y++) {
    const int dy = y - window.cy;
    for (int x = x0; x <= x1; x++) {
      const int dx = x - window.cx;
      const int dist_sq = dx * dx + dy * dy;
      if (dist_sq < min_r_sq || dist_sq > max_r_sq)
        continue;

      const int gx = -filtered[(y - 1) * window.width + (x - 1)] - 2 * filtered[y * window.width + (x - 1)] -
                     filtered[(y + 1) * window.width + (x - 1)] + filtered[(y - 1) * window.width + (x + 1)] +
                     2 * filtered[y * window.width + (x + 1)] + filtered[(y + 1) * window.width + (x + 1)];

      const int gy = -filtered[(y - 1) * window.width + (x - 1)] - 2 * filtered[(y - 1) * window.width + x] -
                     filtered[(y - 1) * window.width + (x + 1)] + filtered[(y + 1) * window.width + (x - 1)] +
                     2 * filtered[(y + 1) * window.width + x] + filtered[(y + 1) * window.width + (x + 1)];

      const int magnitude = static_cast<int>(std::sqrt(static_cast<float>(gx * gx + gy * gy)));
      if (magnitude <= VOTE_MAGNITUDE_MIN)
        continue;

      float theta = radians_to_degrees(std::atan2(static_cast<float>(dy), static_cast<float>(dx)));
      if (theta < 0)
        theta += 360.0f;

      const int vote = std::min(magnitude, 255);
      scores[static_cast<int>(theta) % ANGLE_COUNT] += static_cast<float>(vote);
      total_votes += static_cast<float>(vote);
    }
  }

  float max_votes = 0.0f;
  int best_angle_idx = 0;
  for (int i = 0; i < ANGLE_COUNT; i++) {
    if (scores[i] > max_votes) {
      max_votes = scores[i];
      best_angle_idx = i;
    }
  }

  // Two edges per radius step are budgeted, so support cannot saturate.
  const float support = max_votes / (2.0f * (window.end_r - window.start_r + 1) * 255.0f);
  const float mean = total_votes / static_cast<float>(ANGLE_COUNT);
  const float separation = max_votes > 0.0f ? (max_votes - mean) / max_votes : 0.0f;
  const float confidence = std::clamp(support * separation, 0.0f, 1.0f);

  return {static_cast<float>(best_angle_idx), confidence};
}

}  // namespace

DetectionResult HoughTransformDetector::detect(const uint8_t * /*raw*/, const uint8_t *filtered,
                                               const ScanWindow &window, const DetectorConfig & /*config*/,
                                               float *scores) {
  return detect_hough(filtered, window, scores);
}

#endif  // USE_GAUGE_NEEDLE_HOUGH_TRANSFORM

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
