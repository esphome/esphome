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
                             std::vector<uint8_t> &edges, std::array<int, NUM_ANGLES> &accumulator,
                             int edge_magnitude_min = 30, int vote_magnitude_min = 40, float confidence_scale = 0.1f) {
  if (filtered == nullptr)
    return {};

  const int cx = width / 2;
  const int cy = height / 2;
  const int radius = std::min(cx, cy) - 2;

  // Sobel edge magnitude. The map grows to the largest crop seen and never shrinks.
  const size_t pixels = static_cast<size_t>(width) * height;
  if (edges.size() < pixels)
    edges.resize(pixels);
  std::fill_n(edges.begin(), pixels, 0);

  for (int y = 1; y < height - 1; y++) {
    for (int x = 1; x < width - 1; x++) {
      const int gx = -filtered[(y - 1) * width + (x - 1)] - 2 * filtered[y * width + (x - 1)] -
                     filtered[(y + 1) * width + (x - 1)] + filtered[(y - 1) * width + (x + 1)] +
                     2 * filtered[y * width + (x + 1)] + filtered[(y + 1) * width + (x + 1)];

      const int gy = -filtered[(y - 1) * width + (x - 1)] - 2 * filtered[(y - 1) * width + x] -
                     filtered[(y - 1) * width + (x + 1)] + filtered[(y + 1) * width + (x - 1)] +
                     2 * filtered[(y + 1) * width + x] + filtered[(y + 1) * width + (x + 1)];

      const int magnitude = static_cast<int>(std::sqrt(static_cast<float>(gx * gx + gy * gy)));
      if (magnitude > edge_magnitude_min)
        edges[y * width + x] = static_cast<uint8_t>(std::min<int>(UINT8_MAX, magnitude));
    }
  }

  const int min_r = static_cast<int>(radius * config.min_scan_radius);
  const int max_r = static_cast<int>(radius * config.max_scan_radius);

  std::fill(accumulator.begin(), accumulator.end(), 0);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const uint8_t mag = edges[y * width + x];
      if (mag <= vote_magnitude_min)
        continue;

      const float dx = x - cx;
      const float dy = y - cy;
      const float r = std::sqrt(dx * dx + dy * dy);
      if (r < min_r || r > max_r)
        continue;

      float theta = radians_to_degrees(std::atan2(dy, dx));
      if (theta < 0)
        theta += 360.0f;

      const int angle_idx = static_cast<int>(theta) % NUM_ANGLES;
      accumulator[angle_idx] += mag;
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

  const float confidence = std::min(1.0f, static_cast<float>(max_votes) / (radius * confidence_scale));

  if (config.debug) {
    ESP_LOGD(TAG, "Hough Transform: Best Angle Index=%d, Conf=%.2f", best_angle_idx, confidence);
  }

  return {static_cast<float>(best_angle_idx), confidence};
}

}  // namespace

void HoughTransformDetector::reserve(int width, int height) {
  this->edges_.reserve(static_cast<size_t>(width) * height);
}

DetectionResult HoughTransformDetector::detect(const uint8_t * /*raw*/, const uint8_t *filtered, int width, int height,
                                               const DetectorConfig &config) {
  return detect_hough(filtered, width, height, config, this->edges_, this->accumulator_);
}

#endif  // USE_GAUGE_NEEDLE_HOUGH_TRANSFORM

}  // namespace esphome::gauge_needle_detector

#endif  // USE_GAUGE_NEEDLE_DETECTOR
