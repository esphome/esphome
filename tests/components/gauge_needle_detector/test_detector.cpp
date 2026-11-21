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

/// Crop with a one-pixel needle of value `needle` on a flat `background`.
std::vector<uint8_t> make_plain_crop(float degrees, int background, int needle) {
  std::vector<uint8_t> crop(static_cast<size_t>(WIDTH) * HEIGHT, static_cast<uint8_t>(background));
  const float rad = degrees_to_radians(degrees);
  const float dx = std::cos(rad);
  const float dy = std::sin(rad);
  for (int r = 0; r < NEEDLE_LENGTH; r++) {
    const int x = static_cast<int>(CENTER_X + r * dx);
    const int y = static_cast<int>(CENTER_Y + r * dy);
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
      crop[static_cast<size_t>(y) * WIDTH + x] = static_cast<uint8_t>(needle);
  }
  return crop;
}

/// Gradient background with a needle, so the contrast stretch has something to do.
std::vector<uint8_t> make_textured_crop(float degrees, NeedleType needle_type) {
  std::vector<uint8_t> crop(static_cast<size_t>(WIDTH) * HEIGHT);
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++)
      crop[static_cast<size_t>(y) * WIDTH + x] = static_cast<uint8_t>(40 + (x + y) * 80 / (WIDTH + HEIGHT));
  }
  const float rad = degrees_to_radians(degrees);
  const float dx = std::cos(rad);
  const float dy = std::sin(rad);
  const uint8_t value = needle_type == NEEDLE_TYPE_LIGHT ? 255 : 0;
  for (int r = 0; r < NEEDLE_LENGTH; r++) {
    const int x = static_cast<int>(CENTER_X + r * dx);
    const int y = static_cast<int>(CENTER_Y + r * dy);
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
      crop[static_cast<size_t>(y) * WIDTH + x] = value;
  }
  return crop;
}

DetectorConfig make_config(Algorithm algorithm, NeedleType needle_type) {
  DetectorConfig config;
  config.id = "test";
  config.algorithm = algorithm;
  config.needle_type = needle_type;
  return config;
}

/// Smallest difference between two angles, in degrees.
float angle_difference(float a, float b) {
  const float diff = std::fmod(std::fabs(a - b), 360.0f);
  return diff > 180.0f ? 360.0f - diff : diff;
}

DetectionResult run(const std::vector<uint8_t> &crop, Algorithm algorithm, NeedleType needle_type) {
  NeedleDetector detector;
  EXPECT_TRUE(detector.reserve(WIDTH, HEIGHT));
  return detector.detect(crop.data(), WIDTH, HEIGHT, make_config(algorithm, needle_type));
}

void expect_find(const std::vector<uint8_t> &crop, Algorithm algorithm, NeedleType needle_type, float expected,
                 float tolerance) {
  const DetectionResult result = run(crop, algorithm, needle_type);
  EXPECT_LE(angle_difference(result.angle, expected), tolerance)
      << "algorithm " << LOG_STR_ARG(algorithm_name(algorithm)) << " returned " << result.angle << " degrees";
  EXPECT_GT(result.confidence, 0.0f) << "algorithm " << LOG_STR_ARG(algorithm_name(algorithm))
                                     << " found no confidence";
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
  for (const float angle : {0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 270.0f}) {
    SCOPED_TRACE(::testing::Message() << "angle " << angle);
    expect_find(make_plain_crop(angle, 0, 255), ALGORITHM_LEGACY, NEEDLE_TYPE_LIGHT, angle, 5.0f);
    expect_find(make_plain_crop(angle, 255, 0), ALGORITHM_LEGACY, NEEDLE_TYPE_DARK, angle, 5.0f);
  }
}

TEST(GaugeNeedleDetector, FilteredDetectorsFindTexturedNeedle) {
  for (const NeedleType needle_type : {NEEDLE_TYPE_LIGHT, NEEDLE_TYPE_DARK}) {
    SCOPED_TRACE(::testing::Message() << "needle type " << static_cast<int>(needle_type));
    const std::vector<uint8_t> crop = make_textured_crop(45.0f, needle_type);
    for (const Algorithm algorithm :
         {ALGORITHM_RADIAL_PROFILE, ALGORITHM_HOUGH_TRANSFORM, ALGORITHM_TEMPLATE_MATCH, ALGORITHM_AUTO}) {
      expect_find(crop, algorithm, needle_type, 45.0f, 8.0f);
    }
  }
}

TEST(GaugeNeedleDetector, FilteredDetectorsFindNeedleOnFlatBackground) {
  for (const Algorithm algorithm :
       {ALGORITHM_RADIAL_PROFILE, ALGORITHM_HOUGH_TRANSFORM, ALGORITHM_TEMPLATE_MATCH, ALGORITHM_AUTO}) {
    SCOPED_TRACE(::testing::Message() << "algorithm " << LOG_STR_ARG(algorithm_name(algorithm)));
    expect_find(make_plain_crop(45.0f, 0, 255), algorithm, NEEDLE_TYPE_LIGHT, 45.0f, 8.0f);
    expect_find(make_plain_crop(45.0f, 255, 0), algorithm, NEEDLE_TYPE_DARK, 45.0f, 8.0f);
  }
}

TEST(GaugeNeedleDetector, BlankCropHasNoConfidence) {
  const std::vector<uint8_t> dark(static_cast<size_t>(WIDTH) * HEIGHT, 0);
  const std::vector<uint8_t> light(static_cast<size_t>(WIDTH) * HEIGHT, 255);
  for (const Algorithm algorithm : {ALGORITHM_LEGACY, ALGORITHM_RADIAL_PROFILE, ALGORITHM_HOUGH_TRANSFORM,
                                    ALGORITHM_TEMPLATE_MATCH, ALGORITHM_AUTO}) {
    EXPECT_FLOAT_EQ(0.0f, run(dark, algorithm, NEEDLE_TYPE_LIGHT).confidence)
        << "algorithm " << LOG_STR_ARG(algorithm_name(algorithm));
    EXPECT_FLOAT_EQ(0.0f, run(light, algorithm, NEEDLE_TYPE_DARK).confidence)
        << "algorithm " << LOG_STR_ARG(algorithm_name(algorithm));
  }
}

TEST(GaugeNeedleDetector, DetectRejectsBadInput) {
  const std::vector<uint8_t> crop = make_plain_crop(45.0f, 0, 255);
  NeedleDetector detector;
  EXPECT_FLOAT_EQ(0.0f,
                  detector.detect(nullptr, WIDTH, HEIGHT, make_config(ALGORITHM_LEGACY, NEEDLE_TYPE_LIGHT)).confidence);
  EXPECT_FLOAT_EQ(0.0f,
                  detector.detect(crop.data(), 0, HEIGHT, make_config(ALGORITHM_LEGACY, NEEDLE_TYPE_LIGHT)).confidence);
  EXPECT_FLOAT_EQ(0.0f,
                  detector.detect(crop.data(), WIDTH, 0, make_config(ALGORITHM_LEGACY, NEEDLE_TYPE_LIGHT)).confidence);
}

TEST(GaugeNeedleDetector, TinyCropDoesNotReportConfidence) {
  const std::vector<uint8_t> crop(16, 255);
  for (const Algorithm algorithm : {ALGORITHM_RADIAL_PROFILE, ALGORITHM_HOUGH_TRANSFORM, ALGORITHM_TEMPLATE_MATCH}) {
    NeedleDetector detector;
    ASSERT_TRUE(detector.reserve(4, 4));
    const DetectionResult result = detector.detect(crop.data(), 4, 4, make_config(algorithm, NEEDLE_TYPE_LIGHT));
    EXPECT_TRUE(std::isfinite(result.confidence));
    EXPECT_FLOAT_EQ(0.0f, result.confidence);
  }
}

TEST(GaugeNeedleDetector, PreprocessorKeepsTheNeedle) {
  const std::vector<uint8_t> crop = make_textured_crop(45.0f, NEEDLE_TYPE_LIGHT);
  Preprocessor preprocessor;
  ASSERT_TRUE(preprocessor.reserve(WIDTH, HEIGHT));
  ASSERT_TRUE(preprocessor.prepare(crop.data(), WIDTH, HEIGHT, NEEDLE_LENGTH, NEEDLE_TYPE_LIGHT, false));
  ASSERT_NE(nullptr, preprocessor.data());

  // The one-pixel needle must survive contrast stretch and top-hat.
  size_t surviving = 0;
  const float rad = degrees_to_radians(45.0f);
  for (int r = 0; r < NEEDLE_LENGTH; r++) {
    const int x = static_cast<int>(CENTER_X + r * std::cos(rad));
    const int y = static_cast<int>(CENTER_Y + r * std::sin(rad));
    if (preprocessor.data()[static_cast<size_t>(y) * WIDTH + x] > 0)
      surviving++;
  }
  EXPECT_GT(surviving, static_cast<size_t>(NEEDLE_LENGTH) / 2);
}

}  // namespace esphome::gauge_needle_detector::testing
