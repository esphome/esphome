#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "esphome/components/gauge_needle_detector/gauge_needle_detector.h"

namespace esphome::gauge_needle_detector::testing {

namespace {

constexpr int WIDTH = 64;
constexpr int HEIGHT = 64;
constexpr float CENTER_X = WIDTH / 2.0f;
constexpr float CENTER_Y = HEIGHT / 2.0f;
constexpr int NEEDLE_LENGTH = 30;

/// Black crop with a bright one-pixel needle drawn from the centre outwards.
std::vector<uint8_t> make_dark_crop(float degrees) {
  std::vector<uint8_t> crop(static_cast<size_t>(WIDTH) * HEIGHT, 0);
  const float rad = degrees_to_radians(degrees);
  const float dx = std::cos(rad);
  const float dy = std::sin(rad);
  for (int r = 0; r < NEEDLE_LENGTH; r++) {
    const int x = static_cast<int>(CENTER_X + r * dx);
    const int y = static_cast<int>(CENTER_Y + r * dy);
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
      crop[static_cast<size_t>(y) * WIDTH + x] = 255;
  }
  return crop;
}

/// Dark gradient with a bright needle, so CLAHE has something to stretch.
std::vector<uint8_t> make_textured_crop(float degrees) {
  std::vector<uint8_t> crop = make_dark_crop(degrees);
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      uint8_t &pixel = crop[static_cast<size_t>(y) * WIDTH + x];
      if (pixel == 0)
        pixel = static_cast<uint8_t>(40 + (x + y) * 80 / (WIDTH + HEIGHT));
    }
  }
  return crop;
}

DetectorConfig make_config(Algorithm algorithm) {
  DetectorConfig config;
  config.id = "test";
  config.algorithm = algorithm;
  config.needle_type = NEEDLE_TYPE_LIGHT;
  return config;
}

/// Smallest difference between two angles, in degrees.
float angle_difference(float a, float b) {
  const float diff = std::fmod(std::fabs(a - b), 360.0f);
  return diff > 180.0f ? 360.0f - diff : diff;
}

void expect_angle(const std::vector<uint8_t> &crop, Algorithm algorithm, float expected, float tolerance) {
  NeedleDetector detector;
  detector.reserve(WIDTH, HEIGHT);
  const DetectionResult result = detector.detect(crop.data(), WIDTH, HEIGHT, make_config(algorithm));
  EXPECT_LE(angle_difference(result.angle, expected), tolerance)
      << "algorithm " << LOG_STR_ARG(algorithm_name(algorithm)) << " returned " << result.angle << " degrees";
}

void expect_valid_result(const std::vector<uint8_t> &crop, Algorithm algorithm) {
  NeedleDetector detector;
  detector.reserve(WIDTH, HEIGHT);
  const DetectionResult result = detector.detect(crop.data(), WIDTH, HEIGHT, make_config(algorithm));
  EXPECT_GE(result.confidence, 0.0f);
  EXPECT_LE(result.confidence, 1.0f);
  EXPECT_GE(result.angle, 0.0f);
  EXPECT_LT(result.angle, 360.0f);
}

}  // namespace

TEST(GaugeNeedleDetector, AlgorithmNames) {
  EXPECT_STREQ("legacy", LOG_STR_ARG(algorithm_name(ALGORITHM_LEGACY)));
  EXPECT_STREQ("radial_profile", LOG_STR_ARG(algorithm_name(ALGORITHM_RADIAL_PROFILE)));
  EXPECT_STREQ("hough_transform", LOG_STR_ARG(algorithm_name(ALGORITHM_HOUGH_TRANSFORM)));
  EXPECT_STREQ("template_match", LOG_STR_ARG(algorithm_name(ALGORITHM_TEMPLATE_MATCH)));
  EXPECT_STREQ("auto", LOG_STR_ARG(algorithm_name(ALGORITHM_AUTO)));
}

TEST(GaugeNeedleDetector, IntensityScanFindsRawNeedle) {
  for (const float angle : {0.0f, 45.0f, 90.0f, 180.0f, 270.0f}) {
    expect_angle(make_dark_crop(angle), ALGORITHM_LEGACY, angle, 5.0f);
  }
}

TEST(GaugeNeedleDetector, FilteredDetectorsReturnAValidResult) {
  const std::vector<uint8_t> crop = make_textured_crop(45.0f);
  for (const Algorithm algorithm :
       {ALGORITHM_RADIAL_PROFILE, ALGORITHM_HOUGH_TRANSFORM, ALGORITHM_TEMPLATE_MATCH, ALGORITHM_AUTO}) {
    expect_valid_result(crop, algorithm);
  }
}

TEST(GaugeNeedleDetector, DetectRejectsBadInput) {
  const std::vector<uint8_t> crop = make_dark_crop(45.0f);
  NeedleDetector detector;
  EXPECT_FLOAT_EQ(0.0f, detector.detect(nullptr, WIDTH, HEIGHT, make_config(ALGORITHM_LEGACY)).confidence);
  EXPECT_FLOAT_EQ(0.0f, detector.detect(crop.data(), 0, HEIGHT, make_config(ALGORITHM_LEGACY)).confidence);
  EXPECT_FLOAT_EQ(0.0f, detector.detect(crop.data(), WIDTH, 0, make_config(ALGORITHM_LEGACY)).confidence);
}

TEST(GaugeNeedleDetector, PreprocessorProducesOneBytePerPixel) {
  const std::vector<uint8_t> crop = make_textured_crop(45.0f);
  Preprocessor preprocessor;
  preprocessor.reserve(WIDTH, HEIGHT);
  ASSERT_TRUE(preprocessor.prepare(crop.data(), WIDTH, HEIGHT, NEEDLE_LENGTH, NEEDLE_TYPE_LIGHT, false));
  ASSERT_NE(nullptr, preprocessor.data());

  // The filtered crop still has bright pixels for the detectors to score.
  size_t bright = 0;
  for (size_t i = 0; i < static_cast<size_t>(WIDTH) * HEIGHT; i++) {
    if (preprocessor.data()[i] > 0)
      bright++;
  }
  EXPECT_GT(bright, 0u);
}

}  // namespace esphome::gauge_needle_detector::testing
